# Resume-Board

Diese Datei liest der Team-Lead bei „weiter“ zuerst. Am Ende jedes Arbeitsschritts aktualisieren.

**Stand:** 08.10.2026 · **Meilenstein:** M0 (Einrichtung) · **Status:** M0-06 Engine-Spike in Arbeit

## Zuletzt erledigt

- M0-01 Grundgerüst, Branding, Gate (Doku/Secrets), CI, Git-Hook (Commit `192753d`).
- M0-02 Recherche-Themen, Entscheidungsrunde 1, Design Rev 1.
- M0-03 Vorlage `agent-team-vorlage` **v1.0.0** übernommen (PO-ZIP): Prozessvertrag `team/README.md` mit
  Tracklab-Ergänzungen, Agents `researcher`, `test-writer`, `implementer`, `implementer-rt`, `reviewer`, `cleanup`
  (Modelle/Effort nach Auftrag §3.4), Board als Dateien, `plan/PLAN.md`, `CLAUDE.md` mit Vorlagen-Abschnitt.
  Hook erlaubt jetzt `git merge --ff-only` im eigenen Worktree (F14, Default A).
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

1. M0-06: test-writer (Gerüst + Tests rot) → implementer-rt (grün) → reviewer → Gate → Merge → CI (Windows) prüfen →
   Bericht `team/research/engine-spike/BERICHT.md` → ADR-001 dem PO zur Bestätigung vorlegen (M0-05).
2. M0-07 Gate auf CMake nach dem Spike.
3. M0-08: `daw-features` Teil 1 erledigt (`NOTIZEN.md`); Reaper-Preset und Rest nach F11. `vault-kontext` entfällt (E0b).

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | M0-06 Engine-Spike (Tests fertig auf `m0-06-tests`; implementer-rt läuft) |
| Review | – |
| Backlog | M0-05 ADR-001 · M0-07 Gate CMake (Struktur-Vorschlag steht) · M0-08 Rest-Recherche (daw-features) |
| Erledigt | M0-01 · M0-02 · M0-03 · M0-04 |

## Builds

Noch keine.
