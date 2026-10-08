#!/usr/bin/env bash
# Tracklab gate (Linux / Git Bash). Green = exit 0.
#
# Usage: scripts/gate.sh [static|build|all]   (default: all)
#   static  required files, secrets, audio files, clang-format (no compiler needed)
#   build   CMake Debug + Release (-Werror for our own sources) and ctest; GCC by default (GATE_COMPILER=clang)
#   all     static + build
#
# Rule from agent-team-vorlage: the gate never writes into the working tree. All build output goes to
# GATE_BUILD_DIR (default: ${TMPDIR:-/tmp}/tracklab-gate, one sub directory per CMake preset).
# A missing tool is a failure, never a silent skip.
set -u
cd "$(dirname "$0")/.."

stage="${1:-all}"
GATE_BUILD_DIR="${GATE_BUILD_DIR:-${TMPDIR:-/tmp}/tracklab-gate}"
GATE_COMPILER="${GATE_COMPILER:-gcc}"
# The spike tests treat a missing lame/ffmpeg as a failure when this is set (exit code 77 would mean "skipped").
export SPIKE_REQUIRE_TOOLS=1
# Only the two pinned submodules; Tracktion's own nested JUCE submodule is never initialised.
SUBMODULES="spike/engine/third_party/JUCE spike/engine/third_party/tracktion_engine"

fail=0
ok()   { printf '  [OK]   %s\n' "$1"; }
bad()  { printf '  [FAIL] %s\n' "$1"; fail=1; }
skip() { printf '  [SKIP] %s\n' "$1"; }

# Runs a command, prints how long it took; returns the command's exit status.
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
  [ "$rc" -eq 0 ] || { [ -n "${CI:-}" ] || tail -n 40 "$log"; }
  return $rc
}

stage_static() {
  echo "== Pflichtdateien"
  for f in README.md CLAUDE.md Projektinhalt.md LICENSE .gitignore \
           team/README.md team/RESUME.md team/TODO-PO.md team/ENTSCHEIDUNGEN.md \
           team/plan/PLAN.md team/board/BRIEF-VORLAGE.md \
           team/design/DESIGN.md docs/realtime.md docs/commands.md \
           CMakeLists.txt CMakePresets.json \
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
}

# Presets of the requested compiler family, Debug and Release (DESIGN section 8: both without warnings).
build_presets() {
  case "$GATE_COMPILER" in
    gcc|clang) echo "linux-$GATE_COMPILER-debug linux-$GATE_COMPILER-release" ;;
    *) echo "" ;;
  esac
}

stage_build() {
  echo "== CMake-Build und ctest ($GATE_COMPILER, Debug + Release)"
  local presets p bd missing=0
  presets=$(build_presets)
  if [ -z "$presets" ]; then bad "GATE_COMPILER muss gcc oder clang sein (ist: $GATE_COMPILER)"; return; fi

  for t in cmake ninja ctest; do
    command -v "$t" >/dev/null || { bad "$t nicht installiert"; missing=1; }
  done
  case "$GATE_COMPILER" in
    gcc)   command -v g++ >/dev/null     || { bad "g++ nicht installiert"; missing=1; } ;;
    clang) command -v clang++ >/dev/null || { bad "clang++ nicht installiert"; missing=1; } ;;
  esac
  for d in $SUBMODULES; do
    [ -f "$d/CMakeLists.txt" ] || { bad "Submodul fehlt: $d (git submodule update --init --depth 1 -- $SUBMODULES)"; missing=1; }
  done
  [ "$missing" -eq 0 ] || return

  mkdir -p "$GATE_BUILD_DIR"
  for p in $presets; do
    bd="$GATE_BUILD_DIR/$p"
    echo "-- $p  ($bd)"
    # -B overrides the preset's binaryDir, which points into the working tree.
    logged "configure $p" "$bd.configure.log" cmake --preset "$p" -B "$bd" \
      || { bad "configure $p (Log: $bd.configure.log)"; continue; }
    logged "build $p" "$bd.build.log" cmake --build "$bd" --parallel \
      || { bad "build $p (Log: $bd.build.log)"; continue; }
    ok "build $p"
    timed "ctest $p" ctest --test-dir "$bd" --output-on-failure --output-junit "$bd/junit.xml" \
      && ok "ctest $p" || bad "ctest $p (Bericht: $bd/junit.xml)"
  done
}

case "$stage" in
  static) stage_static ;;
  build)  stage_build ;;
  all)    stage_static; stage_build ;;
  *) echo "Usage: $0 [static|build|all]" >&2; exit 2 ;;
esac

echo
if [ "$fail" -eq 0 ]; then echo "GATE ($stage): GRUEN"; else echo "GATE ($stage): ROT"; fi
exit "$fail"
