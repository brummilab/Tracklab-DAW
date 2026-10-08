# Tracklab gate (Windows PowerShell 7 / Windows PowerShell 5.1). Green = exit 0.
#
# Usage: scripts/gate.ps1 [static|build|all]   (default: all)
#   static  required files, secrets, audio files, clang-format (no compiler needed)
#   build   CMake Debug + Release with MSVC (/W4 /WX for our own sources) and ctest
#   all     static + build
#
# Rule from agent-team-vorlage: the gate never writes into the working tree. All build output goes to
# GATE_BUILD_DIR (default: $env:TEMP\tracklab-gate, one sub directory per CMake preset).
# A missing tool is a failure, never a silent skip. RealtimeSanitizer and clang-tidy are Linux-only stages
# (scripts/gate.sh rtsan / tidy), see docs/realtime.md.
param([ValidateSet('static', 'build', 'all')][string]$Stage = 'all')

$ErrorActionPreference = 'Continue'
Set-Location (Join-Path $PSScriptRoot '..')

$script:fail = 0
function Ok($m)   { Write-Host "  [OK]   $m" }
function Bad($m)  { Write-Host "  [FAIL] $m"; $script:fail = 1 }
function Skip($m) { Write-Host "  [SKIP] $m" }

$GateBuildDir = if ($env:GATE_BUILD_DIR) { $env:GATE_BUILD_DIR } else { Join-Path ([System.IO.Path]::GetTempPath()) 'tracklab-gate' }
# The spike tests treat a missing lame/ffmpeg as a failure when this is set (exit code 77 would mean "skipped").
$env:SPIKE_REQUIRE_TOOLS = '1'
# Configurations of the build stage ("debug release" by default).
$Configs = if ($env:GATE_CONFIGS) { $env:GATE_CONFIGS -split '\s+' | Where-Object { $_ } } else { @('debug', 'release') }
# Only the pinned submodules; Tracktion's own nested JUCE submodule is never initialised.
$Submodules = @('third_party/JUCE', 'third_party/tracktion_engine', 'third_party/nlohmann_json', 'third_party/json-schema-validator')

# Runs a native command (streams its output, which is what a CI log wants) and prints how long it took.
# Returns $true if the exit code was 0.
function Invoke-Timed([string]$Label, [string]$Exe, [string[]]$Arguments) {
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  # Out-Host keeps the command's output out of the function's return value (which must be just the boolean).
  & $Exe @Arguments | Out-Host
  $rc = $LASTEXITCODE
  $sw.Stop()
  Write-Host ("  [TIME] {0}: {1} s" -f $Label, [int]$sw.Elapsed.TotalSeconds)
  return ($rc -eq 0)
}

function Invoke-StageStatic {
  Write-Host '== Pflichtdateien'
  $required = @('README.md','CLAUDE.md','Projektinhalt.md','LICENSE','.gitignore',
    'team/README.md','team/RESUME.md','team/TODO-PO.md','team/ENTSCHEIDUNGEN.md',
    'team/plan/PLAN.md','team/board/BRIEF-VORLAGE.md',
    'team/design/DESIGN.md','docs/realtime.md','docs/commands.md',
    'CMakeLists.txt','CMakePresets.json','.clang-format','.clang-tidy','scripts/rtsan.supp',
    'assets/branding/tracklab-icon.svg','assets/branding/tracklab.ico',
    'assets/branding/png/tracklab-logo-light.png')
  foreach ($f in $required) { if (Test-Path $f) { Ok $f } else { Bad "$f fehlt" } }

  Write-Host '== Agent-Definitionen (model, effort)'
  foreach ($a in @('researcher','test-writer','implementer','implementer-rt','reviewer','cleanup')) {
    $f = ".claude/agents/$a.md"
    if (-not (Test-Path $f)) { Bad "$f fehlt"; continue }
    $h = Get-Content $f -TotalCount 12
    if (($h -match '^model: ') -and ($h -match '^effort: ')) { Ok $f } else { Bad "$f ohne model/effort" }
  }

  Write-Host '== CLAUDE.md unter 200 Zeilen'
  $n = (Get-Content CLAUDE.md -ErrorAction SilentlyContinue | Measure-Object -Line).Lines
  if ($n -lt 200) { Ok "CLAUDE.md: $n Zeilen" } else { Bad "CLAUDE.md: $n Zeilen" }

  Write-Host "== Projektinhalt.md ohne '## Verlauf'"
  if (Select-String -Path Projektinhalt.md -Pattern '^## Verlauf' -Quiet) { Bad "Projektinhalt.md enthaelt '## Verlauf'" } else { Ok 'kein Verlauf-Abschnitt' }

  Write-Host '== Kein repo-lokales core.hooksPath (R6)'
  $hp = git config --local --get core.hooksPath 2>$null
  if (-not $hp) { Ok 'core.hooksPath nicht lokal gesetzt' } else { Bad "core.hooksPath lokal gesetzt: $hp" }

  Write-Host '== Secret-Scan'
  $pattern = 'sk-ant-[A-Za-z0-9_-]{10,}|ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,}'
  $files = git ls-files -co --exclude-standard | Where-Object { $_ -notmatch '^assets/branding/' -and $_ -notmatch '^scripts/gate\.' }
  $hits = @()
  foreach ($f in $files) {
    if ((Test-Path $f -PathType Leaf) -and (Select-String -Path $f -Pattern $pattern -Quiet -ErrorAction SilentlyContinue)) { $hits += $f }
  }
  if ($hits.Count -eq 0) { Ok 'keine Treffer' } else { Bad ("moegliche Secrets in: " + ($hits -join ', ')) }

  Write-Host '== Keine Audio-Mitschnitte im Repo (R13)'
  $audio = git ls-files -co --exclude-standard | Where-Object { $_ -match '\.(wav|mp3|flac|aif|aiff|ogg|opus|m4a)$' -and $_ -notmatch '^tests/fixtures/' }
  if (-not $audio) { Ok 'keine Audiodateien ausserhalb tests/fixtures/' } else { Bad ("Audiodateien gefunden: " + ($audio -join ', ')) }

  # Our own C++ sources (never third_party/): src/, tests/ and the engine spike's src/ and tests/.
  $cf = if ($env:CLANG_FORMAT) { $env:CLANG_FORMAT } else { 'clang-format' }
  Write-Host "== clang-format ($cf)"
  $src = @(git ls-files -co --exclude-standard -- 'src/*.cpp' 'src/*.h' 'src/*.hpp' 'tests/*.cpp' 'tests/*.h' 'tests/*.hpp' `
      'spike/engine/src/*.cpp' 'spike/engine/src/*.h' 'spike/engine/src/*.hpp' `
      'spike/engine/tests/*.cpp' 'spike/engine/tests/*.h' 'spike/engine/tests/*.hpp')
  if (-not (Get-Command $cf -ErrorAction SilentlyContinue)) { Bad "$cf nicht installiert" }
  elseif ($src.Count -eq 0) { Skip 'noch kein C++-Code' }
  else {
    # Judge by the output (like gate.sh): $LASTEXITCODE is not reliable behind a pipeline.
    $out = @(& $cf --dry-run --Werror @src 2>&1)
    if ($out.Count -eq 0) { Ok "$($src.Count) Dateien formatiert" }
    else { $out | Select-Object -First 40 | Out-Host; Bad 'clang-format-Abweichungen (lokal beheben: clang-format -i <Datei>)' }
  }
}

# Makes cl.exe available: in CI the MSVC environment is set up by a workflow step; on a developer machine without
# a Developer PowerShell it is loaded from the newest Visual Studio found by vswhere.
function Initialize-Msvc {
  if (Get-Command cl -ErrorAction SilentlyContinue) { return $true }
  $programFiles = ${env:ProgramFiles(x86)}
  if (-not $programFiles) { return $false }
  $vswhere = Join-Path $programFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (-not (Test-Path $vswhere)) { return $false }
  $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { return $false }
  $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
  if (-not (Test-Path $vcvars)) { return $false }
  # Import the environment that vcvars64.bat sets up into this process.
  cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
  }
  return [bool](Get-Command cl -ErrorAction SilentlyContinue)
}

function Invoke-StageBuild {
  Write-Host '== CMake-Build und ctest (MSVC, Debug + Release)'
  $missing = $false
  foreach ($t in @('cmake', 'ninja', 'ctest')) {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) { Bad "$t nicht installiert"; $missing = $true }
  }
  if (-not (Initialize-Msvc)) { Bad 'MSVC (cl.exe) nicht gefunden: Developer PowerShell oder Visual Studio Build Tools installieren'; $missing = $true }
  foreach ($d in $Submodules) {
    if (-not (Test-Path "$d/CMakeLists.txt")) {
      Bad ("Submodul fehlt: $d (git submodule update --init --depth 1 -- " + ($Submodules -join ' ') + ')'); $missing = $true
    }
  }
  if ($missing) { return }

  # sccache is optional locally (CI installs it and sets SCCACHE_GHA_ENABLED); never a silent skip of the build itself.
  $launcher = @()
  if ((Get-Command sccache -ErrorAction SilentlyContinue) -and -not $env:CMAKE_CXX_COMPILER_LAUNCHER) {
    $launcher = @('-DCMAKE_C_COMPILER_LAUNCHER=sccache', '-DCMAKE_CXX_COMPILER_LAUNCHER=sccache')
  }

  New-Item -ItemType Directory -Force -Path $GateBuildDir | Out-Null
  foreach ($cfg in $Configs) {
    $p = "windows-msvc-$cfg"
    $bd = Join-Path $GateBuildDir $p
    Write-Host "-- $p  ($bd)"
    # -B overrides the preset's binaryDir, which points into the working tree.
    if (-not (Invoke-Timed "configure $p" 'cmake' (@('--preset', $p, '-B', $bd) + $launcher))) { Bad "configure $p"; continue }
    if (-not (Invoke-Timed "build $p" 'cmake' @('--build', $bd, '--parallel'))) { Bad "build $p"; continue }
    Ok "build $p"
    if (Invoke-Timed "ctest $p" 'ctest' @('--test-dir', $bd, '--output-on-failure', '--output-junit', (Join-Path $bd 'junit.xml'))) {
      Ok "ctest $p"
    } else { Bad "ctest $p (Bericht: $(Join-Path $bd 'junit.xml'))" }
  }
}

switch ($Stage) {
  'static' { Invoke-StageStatic }
  'build'  { Invoke-StageBuild }
  'all'    { Invoke-StageStatic; Invoke-StageBuild }
}

Write-Host ''
if ($script:fail -eq 0) { Write-Host "GATE ($Stage): GRUEN" } else { Write-Host "GATE ($Stage): ROT" }
exit $script:fail
