# Resume-Board

Diese Datei liest der Team-Lead bei „weiter“ zuerst. Am Ende jedes Arbeitsschritts aktualisieren.

**Stand:** 08.10.2026 · **Meilenstein:** M1 (Fundament) – Planung · **Status:** M1 läuft – M1-02 + O-04 in Arbeit (M1-06 nach M1-02)

## Zuletzt erledigt

- M0-01 Grundgerüst, Branding, Gate (Doku/Secrets), CI, Git-Hook (Commit `192753d`).
- M0-02 Recherche-Themen, Entscheidungsrunde 1, Design Rev 1.
- M0-03 Vorlage `agent-team-vorlage` **v1.0.0** übernommen (PO-ZIP): Prozessvertrag `team/README.md` mit
  Tracklab-Ergänzungen, Agents `researcher`, `test-writer`, `implementer`, `implementer-rt`, `reviewer`, `cleanup`
  (Modelle/Effort nach Auftrag §3.4), Board als Dateien, `plan/PLAN.md`, `CLAUDE.md` mit Vorlagen-Abschnitt.
  Hook erlaubt jetzt `git merge --ff-only` im eigenen Worktree (F14, Default A).
- **M1-01** gemerged (`2097ea1`): Submodule unter `third_party/`, `cmake/TracklabDeps.cmake` (JUCE/Tracktion einmal
  kompiliert, Spike + `src/` teilen), `src/engine` Engine-Fabrik (Settings atomar/in-memory, headless UIBehaviour,
  `getUserName()`="Tracklab"), `tracklab_tests`. Gate all grün. Folgekarte O-04.
- **M0-07 Gate auf CMake** gemerged (`f0d26f0`): Root-CMake + Presets; `gate.sh static|build|tidy|rtsan|all`,
  `gate.ps1` (MSVC); clang-format/-tidy nur eigener Code; RTSan (Clang 20) mit 20 Fremdcode-Suppressions, Negativtest,
  `-Wfunction-effects`; ein Workflow `gate.yml`. Review 2 Runden (`team/reviews/M0-07.md`). Folgekarte O-02.
- **M0-06 Engine-Spike** gemerged (`33c5eff`): JUCE 9.0.3 + Tracktion `develop` bauen zusammen; 57/57 Tests grün
  (GCC/Clang); Bericht `team/research/engine-spike/BERICHT.md`; Review 2 Runden (`team/reviews/M0-06.md`).
  **CI grün** inkl. Windows/MSVC, MP3 auf 6 Kombinationen bitidentisch (Run 37762180378).
  Folgekarten M2-01 (MP3 gapless), M4-01 (Latenz-Blockversatz), O-01 (CI-Pakete, Upstream-Issue).
- Entscheidungsrunden 1+2: „Defaults ok“ → E1–E38; Lizenz AGPLv3 (`LICENSE`); Design Rev 2; ADR-001-Entwurf
  (`docs/adr/ADR-001-tech-stack.md`); Präfixregel E15 in `team/README.md`.
- M0-04 Recherche: 10 Themen mit `NOTIZEN.md` (Quellen, Stand 08.10.2026). Wichtigste Befunde: **JUCE 9.0.3 ist
  erschienen** (Auftrag §5 veraltet); Tracktion `develop` = 3.5.0 ungetaggt mit LUFS/Render-Queue/Mehrkanal; MCP-Spec
  2026-07-28 (zustandslos), kein C++-SDK; Claude-API max. 20 strict-Tools, `tool_choice any/tool` auf Opus/Sonnet 5.5 verboten;
  AGPLv3 bestätigt, Namenstreffer 2Simple „Tracklab“; JUCE-ALSA ohne RT-Thread → JACK über PipeWire; kein CLAP in JUCE 9;
  Screenshots laufen ohne Display. Brief-Vorschläge in M0-06 und M0-07. Entscheidungsrunde 2 (F16–F38).

## Wartet auf den PO

- Infos ohne Default: F9 Audio-Interface, F10 Reaper-MCP, F11 `reaper-kb.ini`, F12 lokal/Cloud, F22 Test-API-Key,
  F28b EBU-Quellen (optional).

## Nächster Schritt (Lead)

1. M1 läuft nach Abhängigkeiten (max. 4 parallel): **M1-01** zuerst → dann M1-02 und M1-06 parallel → M1-03 → M1-04 →
   M1-05 und M1-07 → M1-08 (nach M1-02/M1-04) → M1-09. Je Karte: test-writer → implementer → reviewer → Gate → Merge → CI.
2. M0-08 (Reaper-Preset) ruht bis F11; Umsetzung M9. O-02 vor Karten mit eigenen Graph-Nodes; O-03 vor Mint-23-Support.

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | M1-02 Registry (implementer) · O-04 Nacharbeiten M1-01 (implementer) |
| Review | – |
| Backlog | M1-03 … M1-09 · M0-08 Rest-Recherche (daw-features) · M2-01 · M4-01 · O-01 · O-02 · O-03 |
| Erledigt | M0-01 · M0-02 · M0-03 · M0-04 · M0-05 · M0-06 · M0-07 · M1-01 |

## Builds

Spike-Artefakte (`spike_cli`, `SpikeGain.vst3`) ab dem ersten Lauf von Workflow `spike-engine` auf `main` (GitHub Actions).
