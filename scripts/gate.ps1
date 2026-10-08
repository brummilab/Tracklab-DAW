# Tracklab gate (Windows PowerShell). Green = exit 0.
# M0 stage: documentation and secret checks only, same as scripts/gate.sh.
$ErrorActionPreference = 'Continue'
Set-Location (Join-Path $PSScriptRoot '..')

$script:fail = 0
function Ok($m)   { Write-Host "  [OK]   $m" }
function Bad($m)  { Write-Host "  [FAIL] $m"; $script:fail = 1 }
function Skip($m) { Write-Host "  [SKIP] $m" }

Write-Host '== Pflichtdateien'
$required = @('README.md','CLAUDE.md','Projektinhalt.md','LICENSE','.gitignore',
  'team/RESUME.md','team/TODO-PO.md','team/ENTSCHEIDUNGEN.md','team/BOARD.md',
  'team/design/DESIGN.md','docs/realtime.md','docs/commands.md',
  'assets/branding/tracklab-icon.svg','assets/branding/tracklab.ico',
  'assets/branding/png/tracklab-logo-light.png')
foreach ($f in $required) { if (Test-Path $f) { Ok $f } else { Bad "$f fehlt" } }

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
  if ((Test-Path $f) -and (Select-String -Path $f -Pattern $pattern -Quiet -ErrorAction SilentlyContinue)) { $hits += $f }
}
if ($hits.Count -eq 0) { Ok 'keine Treffer' } else { Bad ("moegliche Secrets in: " + ($hits -join ', ')) }

Write-Host '== Keine Audio-Mitschnitte im Repo (R13)'
$audio = git ls-files -co --exclude-standard | Where-Object { $_ -match '\.(wav|mp3|flac|aif|aiff|ogg|opus|m4a)$' -and $_ -notmatch '^tests/fixtures/' }
if (-not $audio) { Ok 'keine Audiodateien ausserhalb tests/fixtures/' } else { Bad ("Audiodateien gefunden: " + ($audio -join ', ')) }

Write-Host '== clang-format'
$src = git ls-files -co --exclude-standard -- 'src/*.cpp' 'src/*.h' 'src/*.hpp' 'tests/*.cpp' 'tests/*.h'
if (-not $src) { Skip 'noch kein C++-Code' }
elseif (Get-Command clang-format -ErrorAction SilentlyContinue) {
  clang-format --dry-run --Werror @src
  if ($LASTEXITCODE -eq 0) { Ok 'formatiert' } else { Bad 'clang-format-Abweichungen' }
} else { Bad 'clang-format nicht installiert' }

Write-Host '== CMake-Build, ctest, Golden'
if (Test-Path CMakeLists.txt) { Bad 'CMakeLists.txt vorhanden, Build-Schritte im Gate aber noch nicht aktiviert' }
else { Skip 'folgt mit dem Engine-Spike (M0)' }

Write-Host ''
if ($script:fail -eq 0) { Write-Host 'GATE: GRUEN' } else { Write-Host 'GATE: ROT' }
exit $script:fail
