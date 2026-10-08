# Rückblick M0 – Einrichtung

Stand: 08.10.2026 · Lead: Hauptsitzung · PO: David · Abschluss: CI grün (Run 37783956619, alle Jobs)

## Zahlen
- **Karten erledigt:** 7 (M0-01 Grundgerüst, M0-02 Recherche-Themen/Design Rev 1, M0-03 Vorlage, M0-04 Recherche,
  M0-05 ADR-001, M0-06 Engine-Spike, M0-07 Gate). Teilweise: M0-08 (Reaper-Preset wartet auf F11).
- **Entscheidungen:** E0, E0b, E1–E8, E13–E21, E23–E39 (34 F-Fragen über zwei Runden + ADR-Bestätigung).
- **Recherche:** 11 Themen mit `NOTIZEN.md`, alle Befunde mit Quelle und Stand-Datum.
- **Reviews:** M0-06 zwei Runden (1× Nacharbeit), M0-07 drei Runden (1× Nacharbeit, 1× CI-Fix).
- **Tests:** 57 doctest-Fälle (Spike) + RTSan-Negativtest; CI: Linux GCC/Clang, Windows MSVC, RTSan (Clang 20).
- **Folgekarten:** M2-01 (MP3 gapless), M4-01 (Latenz-Blockversatz), O-01 (CI-Pakete/Actions/Doppelläufe),
  O-02 (RTSan-Strategie, Build-Ordner), O-03 (libstdc++ 15 / Mint 23).

## Was lief gut
- **Recherche vor Entscheidung:** Sie hat mehrere falsche Annahmen des Auftrags früh korrigiert (JUCE 9 erschienen,
  `juce_clap_hosting` existiert nicht, `strict`-Limit 20, `tool_choice` auf 5.5er-Modellen, Screenreader nur Windows).
- **Defaults mit Empfehlung:** Zwei Entscheidungsrunden mit 38 Fragen ließen sich mit „Defaults ok“ abschließen.
- **Reviewer mit konkreten Prüfpunkten:** fand eine falsche Sicherheitsaussage (`-Wfunction-effects` nicht aktiv), eine
  fehlende Begründung und eine abgeschaltete Cache-API. Eigene Nachmessungen statt Vertrauen in den Bericht.
- **Test-Writer → Implementer:** Ein Test war nachweislich falsch (True Peak mit harten Kanten) und wurde mit Beleg
  korrigiert, ohne die Toleranz aufzuweichen.
- **Spike headless:** Alle Kernfunktionen (Import, Render + LUFS, 12 Eingänge, VST3) laufen ohne Gerät und ohne Display;
  MP3 auf allen Plattformen bitidentisch.

## Was hakte
- **Turn-Limits der Sub-Agents:** Fünf Läufe brachen am Limit ab, einer am API-Sitzungslimit. Gegenmittel ab M1:
  Briefs kleiner schneiden; im Auftrag immer „zuerst committen, dann Teilbericht“.
- **Gesperrte Primärquellen** (EBU, ITU, juce.com, Microsoft, Reaper, Steinberg): viele `[VERIFIZIEREN]`. F28b bleibt offen.
- **CI erst nach dem Merge:** Windows und neue Runner (ubuntu-26.04) ließen sich nur auf `main` prüfen; der erste
  Gate-Lauf war deshalb teilweise rot. Gegenmittel: CI-Änderungen kleiner und mit Fallback planen.
- **Gemeinsames Build-Verzeichnis** zwischen Worktree und Haupt-Checkout machte das Gate fälschlich rot (O-02).
- **Doppelte CI-Läufe** durch Push auf `main` und Arbeitsbranch (O-01).

## Prozessanpassungen für M1
1. Briefs auf ≤ ~1 Tag Agent-Arbeit schneiden; mehrere kleine Karten statt einer großen.
2. Jeder Auftrag an Sub-Agents endet mit: „Budget knapp → zuerst committen, dann Teilbericht.“
3. Reviewer prüft ab M1 zusätzlich: `RealtimeScope` nur an Grenzfunktionen; clang-tidy-Abschaltungen für `src/` neu bewerten.
4. CI-Änderungen: immer mit dokumentiertem Fallback-Runner.
