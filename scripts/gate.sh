#!/usr/bin/env bash
# Tracklab gate (Linux / Git Bash). Green = exit 0.
#
# Usage: scripts/gate.sh [static|build|tidy|rtsan|all]   (default: all)
#   static  required files, secrets, audio files, clang-format (no compiler needed)
#   build   CMake Debug + Release (-Werror for our own sources) and ctest; GCC by default (GATE_COMPILER=clang)
#   tidy    clang-tidy over our own sources (needs the compile database of the linux-clang-debug preset)
#   rtsan   RealtimeSanitizer run with Clang >= 20 (docs/realtime.md); SKIP if there is no such Clang,
#           FAIL instead of SKIP when GATE_REQUIRE_RTSAN=1 (CI)
#   all     static + build + tidy + rtsan
#
# Environment:
#   GATE_BUILD_DIR      build output, default ${TMPDIR:-/tmp}/tracklab-gate (one sub directory per CMake preset)
#   GATE_COMPILER       gcc (default) or clang, selects the presets of the build stage
#   GATE_CONFIGS        configurations of the build stage, default "debug release"
#   GATE_CC, GATE_CXX   compiler override for the build/tidy stage (e.g. gcc-14 / g++-14), the preset's name otherwise
#   GATE_CLANG          clang (>= 20) for the rtsan stage, default: first of clang-22, clang-21, clang-20, clang
#   GATE_REQUIRE_RTSAN  1 = a missing Clang >= 20 is a failure
#   CLANG_FORMAT, CLANG_TIDY   tool names, default clang-format / clang-tidy
#
# Rule from agent-team-vorlage: the gate never writes into the working tree. A missing tool is a failure,
# never a silent skip (exception: rtsan locally, see above).
set -u
cd "$(dirname "$0")/.."

stage="${1:-all}"
GATE_BUILD_DIR="${GATE_BUILD_DIR:-${TMPDIR:-/tmp}/tracklab-gate}"
GATE_COMPILER="${GATE_COMPILER:-gcc}"
GATE_CONFIGS="${GATE_CONFIGS:-debug release}"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
# The spike tests treat a missing lame/ffmpeg as a failure when this is set (exit code 77 would mean "skipped").
export SPIKE_REQUIRE_TOOLS=1
# Only the two pinned submodules; Tracktion's own nested JUCE submodule is never initialised.
SUBMODULES="spike/engine/third_party/JUCE spike/engine/third_party/tracktion_engine"

fail=0
ok()   { printf '  [OK]   %s\n' "$1"; }
bad()  { printf '  [FAIL] %s\n' "$1"; fail=1; }
skip() { printf '  [SKIP] %s\n' "$1"; }

# Runs a command and prints how long it took; returns the command's exit status.
timed() {
  local label="$1" start rc
  shift
  start=$(date +%s)
  "$@"
  rc=$?
  printf '  [TIME] %s: %s s\n' "$label" "$(( $(date +%s) - start ))"
  return $rc
}

# Runs a command with its output in a log file (kept quiet locally, streamed to the console in CI), times it.
logged() {
  local label="$1" log="$2" start rc
  shift 2
  start=$(date +%s)
  if [ -n "${CI:-}" ]; then
    "$@" 2>&1 | tee "$log"
    rc=${PIPESTATUS[0]}
  else
    "$@" >"$log" 2>&1
    rc=$?
  fi
  printf '  [TIME] %s: %s s\n' "$label" "$(( $(date +%s) - start ))"
  [ "$rc" -eq 0 ] || { [ -z "${CI:-}" ] && tail -n 40 "$log"; }
  return $rc
}

# Our own C++ sources (never third_party/): src/, tests/ and the engine spike's src/ and tests/.
own_sources() {
  git ls-files -co --exclude-standard -- \
    'src/*.cpp' 'src/*.h' 'src/*.hpp' 'tests/*.cpp' 'tests/*.h' 'tests/*.hpp' \
    'spike/engine/src/*.cpp' 'spike/engine/src/*.h' 'spike/engine/src/*.hpp' \
    'spike/engine/tests/*.cpp' 'spike/engine/tests/*.h' 'spike/engine/tests/*.hpp'
}

# Checks that the pinned submodules are there (the gate does not fetch: that would write into the working tree).
require_submodules() {
  local d rc=0
  for d in $SUBMODULES; do
    [ -f "$d/CMakeLists.txt" ] || { bad "Submodul fehlt: $d (git submodule update --init --depth 1 -- $SUBMODULES)"; rc=1; }
  done
  return $rc
}

# Compiler override for the configure step: GATE_CC / GATE_CXX or the explicit arguments of the caller.
compiler_args() {
  local cc="${1:-${GATE_CC:-}}" cxx="${2:-${GATE_CXX:-}}"
  [ -n "$cc" ]  && printf '%s\n' "-DCMAKE_C_COMPILER=$cc"
  [ -n "$cxx" ] && printf '%s\n' "-DCMAKE_CXX_COMPILER=$cxx"
  return 0
}

stage_static() {
  echo "== Pflichtdateien"
  for f in README.md CLAUDE.md Projektinhalt.md LICENSE .gitignore \
           team/README.md team/RESUME.md team/TODO-PO.md team/ENTSCHEIDUNGEN.md \
           team/plan/PLAN.md team/board/BRIEF-VORLAGE.md \
           team/design/DESIGN.md docs/realtime.md docs/commands.md \
           CMakeLists.txt CMakePresets.json .clang-format .clang-tidy scripts/rtsan.supp \
           assets/branding/tracklab-icon.svg assets/branding/tracklab.ico \
           assets/branding/png/tracklab-logo-light.png; do
    [ -f "$f" ] && ok "$f" || bad "$f fehlt"
  done

  echo "== Agent-Definitionen (model, effort)"
  for a in researcher test-writer implementer implementer-rt reviewer cleanup; do
    f=".claude/agents/$a.md"
    if [ ! -f "$f" ]; then bad "$f fehlt"; continue; fi
    head -n 12 "$f" | grep -q '^model: ' && head -n 12 "$f" | grep -q '^effort: ' && ok "$f" || bad "$f ohne model/effort"
  done

  echo "== CLAUDE.md unter 200 Zeilen"
  n=$(wc -l < CLAUDE.md 2>/dev/null || echo 9999)
  [ "$n" -lt 200 ] && ok "CLAUDE.md: $n Zeilen" || bad "CLAUDE.md: $n Zeilen"

  echo "== Projektinhalt.md ohne '## Verlauf' (den schreibt der Vault-Hook)"
  grep -q '^## Verlauf' Projektinhalt.md 2>/dev/null && bad "Projektinhalt.md enthaelt '## Verlauf'" || ok "kein Verlauf-Abschnitt"

  echo "== Kein repo-lokales core.hooksPath (R6)"
  hp=$(git config --local --get core.hooksPath 2>/dev/null || true)
  [ -z "$hp" ] && ok "core.hooksPath nicht lokal gesetzt" || bad "core.hooksPath lokal gesetzt: $hp"

  echo "== Secret-Scan"
  # Tracked and untracked (not ignored) files; binary assets are skipped by grep -I.
  # Third-party submodules are not ours and are not listed by ls-files (gitlinks).
  pattern='(sk-ant-[A-Za-z0-9_-]{10,}|ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,})'
  hits=$(git ls-files -co --exclude-standard -z | grep -zv '^assets/branding/' | grep -zv '^scripts/gate\.' \
         | xargs -0 -r grep -IEl "$pattern" 2>/dev/null || true)
  [ -z "$hits" ] && ok "keine Treffer" || bad "moegliche Secrets in: $hits"

  echo "== Keine Audio-Mitschnitte im Repo (R13)"
  audio=$(git ls-files -co --exclude-standard | grep -Ei '\.(wav|mp3|flac|aif|aiff|ogg|opus|m4a)$' | grep -v '^tests/fixtures/' || true)
  [ -z "$audio" ] && ok "keine Audiodateien ausserhalb tests/fixtures/" || bad "Audiodateien gefunden: $audio"

  echo "== clang-format ($CLANG_FORMAT)"
  src=$(own_sources)
  if ! command -v "$CLANG_FORMAT" >/dev/null; then
    bad "$CLANG_FORMAT nicht installiert"
  elif [ -z "$src" ]; then
    skip "noch kein C++-Code"
  else
    # shellcheck disable=SC2086
    if $CLANG_FORMAT --dry-run --Werror $src 2>&1 | head -n 40 | grep -q .; then
      # shellcheck disable=SC2086
      $CLANG_FORMAT --dry-run --Werror $src 2>&1 | head -n 40
      bad "clang-format-Abweichungen (lokal beheben: clang-format -i <Datei>)"
    else
      ok "$(echo "$src" | wc -l) Dateien formatiert"
    fi
  fi
}

stage_build() {
  echo "== CMake-Build und ctest ($GATE_COMPILER: $GATE_CONFIGS)"
  local cfg p bd missing=0
  case "$GATE_COMPILER" in
    gcc)   command -v "${GATE_CXX:-g++}" >/dev/null     || { bad "${GATE_CXX:-g++} nicht installiert"; missing=1; } ;;
    clang) command -v "${GATE_CXX:-clang++}" >/dev/null || { bad "${GATE_CXX:-clang++} nicht installiert"; missing=1; } ;;
    *) bad "GATE_COMPILER muss gcc oder clang sein (ist: $GATE_COMPILER)"; return ;;
  esac
  for t in cmake ninja ctest; do
    command -v "$t" >/dev/null || { bad "$t nicht installiert"; missing=1; }
  done
  require_submodules || missing=1
  [ "$missing" -eq 0 ] || return

  mkdir -p "$GATE_BUILD_DIR"
  for cfg in $GATE_CONFIGS; do
    p="linux-$GATE_COMPILER-$cfg"
    bd="$GATE_BUILD_DIR/$p"
    echo "-- $p  ($bd)"
    # -B overrides the preset's binaryDir, which points into the working tree.
    # shellcheck disable=SC2046
    logged "configure $p" "$bd.configure.log" cmake --preset "$p" -B "$bd" $(compiler_args) \
      || { bad "configure $p (Log: $bd.configure.log)"; continue; }
    logged "build $p" "$bd.build.log" cmake --build "$bd" --parallel \
      || { bad "build $p (Log: $bd.build.log)"; continue; }
    ok "build $p"
    timed "ctest $p" ctest --test-dir "$bd" --output-on-failure --output-junit "$bd/junit.xml" \
      && ok "ctest $p" || bad "ctest $p (Bericht: $bd/junit.xml)"
  done
}

stage_tidy() {
  echo "== clang-tidy ($CLANG_TIDY)"
  local bd="$GATE_BUILD_DIR/linux-clang-debug" files missing=0
  command -v "$CLANG_TIDY" >/dev/null || { bad "$CLANG_TIDY nicht installiert"; missing=1; }
  for t in cmake ninja "${GATE_CXX:-clang++}"; do
    command -v "$t" >/dev/null || { bad "$t nicht installiert"; missing=1; }
  done
  require_submodules || missing=1
  [ "$missing" -eq 0 ] || return

  mkdir -p "$GATE_BUILD_DIR"
  # The compile database comes from the Clang preset (clang-tidy understands Clang's flags); configure is enough,
  # nothing has to be built. A build of the same preset (CI) is reused.
  # shellcheck disable=SC2046
  logged "configure linux-clang-debug" "$bd.configure.log" cmake --preset linux-clang-debug -B "$bd" $(compiler_args) \
    || { bad "configure linux-clang-debug (Log: $bd.configure.log)"; return; }

  files=$(own_sources | grep '\.cpp$')
  [ -n "$files" ] || { skip "keine C++-Quellen"; return; }
  # Only our headers are reported (system headers are never reported), one clang-tidy process per core.
  # shellcheck disable=SC2016
  if timed "clang-tidy" bash -c 'printf "%s\n" "$1" | xargs -P "$(nproc)" -n 1 "$2" -p "$3" --quiet --header-filter="$4" 2> >(grep -Ev "^[0-9]+ warnings? generated\.$" >&2)' \
       _ "$files" "$CLANG_TIDY" "$bd" "^$(pwd)/(spike/engine/)?(src|tests)/"; then
    ok "clang-tidy: $(echo "$files" | wc -l) Quellen ohne Befund"
  else
    bad "clang-tidy-Befunde"
  fi
}

# Prints "<clang> <clang++> <major>" of the first Clang >= 20, empty if there is none.
find_rtsan_clang() {
  local c major cxx
  local candidates="clang-22 clang-21 clang-20 clang"
  # A pinned GATE_CLANG is the only candidate: the suppressions are verified for one version.
  [ -n "${GATE_CLANG:-}" ] && candidates="$GATE_CLANG"
  for c in $candidates; do
    [ -n "$c" ] && command -v "$c" >/dev/null || continue
    major=$(echo | "$c" -dM -E -x c - 2>/dev/null | awk '/__clang_major__/ {print $3}')
    if [ -n "$major" ] && [ "$major" -ge 20 ]; then
      cxx="${c/clang/clang++}"
      command -v "$cxx" >/dev/null || continue
      echo "$c $cxx $major"
      return 0
    fi
  done
  return 1
}

stage_rtsan() {
  echo "== RealtimeSanitizer (Clang >= 20, docs/realtime.md)"
  local found cc cxx major symbolizer s bd="$GATE_BUILD_DIR/linux-clang-rtsan" missing=0
  if ! found=$(find_rtsan_clang); then
    if [ "${GATE_REQUIRE_RTSAN:-0}" = 1 ]; then
      bad "kein Clang >= 20 gefunden (GATE_REQUIRE_RTSAN=1)"
    else
      skip "kein Clang >= 20 gefunden (Ubuntu: sudo apt install clang-20 libclang-rt-20-dev llvm-20; in CI Pflicht)"
    fi
    return
  fi
  read -r cc cxx major <<<"$found"
  echo "-- Compiler: $cc ($($cc --version | head -n 1))"
  # RTSan reports are matched against scripts/rtsan.supp by function name, so the symbolizer is mandatory.
  symbolizer=""
  for s in "llvm-symbolizer-$major" "/usr/lib/llvm-$major/bin/llvm-symbolizer" llvm-symbolizer; do
    if command -v "$s" >/dev/null; then symbolizer=$(command -v "$s"); break; fi
  done
  [ -n "$symbolizer" ] || { bad "llvm-symbolizer nicht gefunden (Ubuntu: sudo apt install llvm-$major)"; missing=1; }
  for t in cmake ninja ctest; do
    command -v "$t" >/dev/null || { bad "$t nicht installiert"; missing=1; }
  done
  require_submodules || missing=1
  [ "$missing" -eq 0 ] || return

  mkdir -p "$GATE_BUILD_DIR"
  logged "configure linux-clang-rtsan" "$bd.configure.log" \
    cmake --preset linux-clang-rtsan -B "$bd" "-DCMAKE_C_COMPILER=$cc" "-DCMAKE_CXX_COMPILER=$cxx" \
    "-DTRACKLAB_RTSAN_SYMBOLIZER=$symbolizer" \
    || { bad "configure linux-clang-rtsan (Log: $bd.configure.log)"; return; }
  logged "build linux-clang-rtsan" "$bd.build.log" cmake --build "$bd" --parallel \
    || { bad "build linux-clang-rtsan (Log: $bd.build.log)"; return; }
  ok "build linux-clang-rtsan"
  # RTSAN_OPTIONS and the suppressions file are set per test in spike/engine/CMakeLists.txt.
  timed "ctest linux-clang-rtsan" ctest --test-dir "$bd" --output-on-failure --output-junit "$bd/junit.xml" \
    && ok "ctest linux-clang-rtsan" || bad "ctest linux-clang-rtsan (Bericht: $bd/junit.xml)"
}

case "$stage" in
  static) stage_static ;;
  build)  stage_build ;;
  tidy)   stage_tidy ;;
  rtsan)  stage_rtsan ;;
  all)    stage_static; stage_build; stage_tidy; stage_rtsan ;;
  *) echo "Usage: $0 [static|build|tidy|rtsan|all]" >&2; exit 2 ;;
esac

echo
if [ "$fail" -eq 0 ]; then echo "GATE ($stage): GRUEN"; else echo "GATE ($stage): ROT"; fi
exit "$fail"
