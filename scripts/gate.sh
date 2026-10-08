#!/usr/bin/env bash
# Tracklab gate (Linux / Git Bash). Green = exit 0.
# M0 stage: documentation and secret checks only. The CMake/ctest/RTSan/
# clang-tidy steps are activated together with the engine spike (see
# team/design/DESIGN.md, section "Gate").
set -u
cd "$(dirname "$0")/.."

fail=0
ok()   { printf '  [OK]   %s\n' "$1"; }
bad()  { printf '  [FAIL] %s\n' "$1"; fail=1; }
skip() { printf '  [SKIP] %s\n' "$1"; }

echo "== Pflichtdateien"
for f in README.md CLAUDE.md Projektinhalt.md LICENSE .gitignore \
         team/RESUME.md team/TODO-PO.md team/ENTSCHEIDUNGEN.md team/BOARD.md \
         team/design/DESIGN.md docs/realtime.md docs/commands.md \
         assets/branding/tracklab-icon.svg assets/branding/tracklab.ico \
         assets/branding/png/tracklab-logo-light.png; do
  [ -f "$f" ] && ok "$f" || bad "$f fehlt"
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
pattern='(sk-ant-[A-Za-z0-9_-]{10,}|ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[baprs]-[A-Za-z0-9-]{10,})'
hits=$(git ls-files -co --exclude-standard -z | grep -zv '^assets/branding/' | grep -zv '^scripts/gate\.' \
       | xargs -0 -r grep -IEl "$pattern" 2>/dev/null || true)
[ -z "$hits" ] && ok "keine Treffer" || bad "moegliche Secrets in: $hits"

echo "== Keine Audio-Mitschnitte im Repo (R13)"
audio=$(git ls-files -co --exclude-standard | grep -Ei '\.(wav|mp3|flac|aif|aiff|ogg|opus|m4a)$' | grep -v '^tests/fixtures/' || true)
[ -z "$audio" ] && ok "keine Audiodateien ausserhalb tests/fixtures/" || bad "Audiodateien gefunden: $audio"

echo "== clang-format"
src=$(git ls-files -co --exclude-standard -- 'src/*.cpp' 'src/*.h' 'src/*.hpp' 'tests/*.cpp' 'tests/*.h' 2>/dev/null || true)
if [ -z "$src" ]; then
  skip "noch kein C++-Code"
elif command -v clang-format >/dev/null; then
  # shellcheck disable=SC2086
  clang-format --dry-run --Werror $src && ok "formatiert" || bad "clang-format-Abweichungen"
else
  bad "clang-format nicht installiert"
fi

echo "== CMake-Build, ctest, Golden, LUFS, RTSan, clang-tidy"
if [ -f CMakeLists.txt ]; then
  bad "CMakeLists.txt vorhanden, Build-Schritte im Gate aber noch nicht aktiviert"
else
  skip "folgt mit dem Engine-Spike (M0)"
fi

echo
if [ "$fail" -eq 0 ]; then echo "GATE: GRUEN"; else echo "GATE: ROT"; fi
exit "$fail"
