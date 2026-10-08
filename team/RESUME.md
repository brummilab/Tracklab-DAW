# Resume-Board

Diese Datei liest der Team-Lead bei „weiter“ zuerst. Am Ende jedes Arbeitsschritts aktualisieren.

**Stand:** 08.10.2026 · **Meilenstein:** M0 (Einrichtung) · **Status:** M0-07 Gate auf CMake in Arbeit · ADR-001 angenommen (E39)

## Zuletzt erledigt

- M0-01 Grundgerüst, Branding, Gate (Doku/Secrets), CI, Git-Hook (Commit `192753d`).
- M0-02 Recherche-Themen, Entscheidungsrunde 1, Design Rev 1.
- M0-03 Vorlage `agent-team-vorlage` **v1.0.0** übernommen (PO-ZIP): Prozessvertrag `team/README.md` mit
  Tracklab-Ergänzungen, Agents `researcher`, `test-writer`, `implementer`, `implementer-rt`, `reviewer`, `cleanup`
  (Modelle/Effort nach Auftrag §3.4), Board als Dateien, `plan/PLAN.md`, `CLAUDE.md` mit Vorlagen-Abschnitt.
  Hook erlaubt jetzt `git merge --ff-only` im eigenen Worktree (F14, Default A).
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

1. M0-07: implementer läuft → reviewer → Gate → Merge → CI prüfen.
2. Danach M0-Abschluss: Rückblick `team/RUECKBLICK-M0.md`, M1 planen (Briefs).
3. M0-08: Reaper-Preset nach F11.

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | – |
| Review | M0-07 Gate (Runde 1: Nacharbeit -Wfunction-effects) |
| Backlog | M0-08 Rest-Recherche (daw-features) · M2-01 · M4-01 · O-01 |
| Erledigt | M0-01 · M0-02 · M0-03 · M0-04 · M0-05 · M0-06 |

## Builds

Spike-Artefakte (`spike_cli`, `SpikeGain.vst3`) ab dem ersten Lauf von Workflow `spike-engine` auf `main` (GitHub Actions).
