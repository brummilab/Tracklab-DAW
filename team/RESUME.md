# Resume-Board

Diese Datei liest der Team-Lead bei „weiter“ zuerst. Am Ende jedes Arbeitsschritts aktualisieren.

**Stand:** 08.10.2026 · **Meilenstein:** M0 (Einrichtung) · **Status: BLOCKED: USER INPUT REQUIRED** (`TODO-PO.md`)

## Zuletzt erledigt

- M0-01 Grundgerüst, Branding, Gate (Doku/Secrets), CI, Git-Hook (Commit `192753d`).
- M0-02 Recherche-Themen, Entscheidungsrunde 1, Design Rev 1.
- M0-03 Vorlage `agent-team-vorlage` **v1.0.0** übernommen (PO-ZIP): Prozessvertrag `team/README.md` mit
  Tracklab-Ergänzungen, Agents `researcher`, `test-writer`, `implementer`, `implementer-rt`, `reviewer`, `cleanup`
  (Modelle/Effort nach Auftrag §3.4), Board als Dateien, `plan/PLAN.md`, `CLAUDE.md` mit Vorlagen-Abschnitt.
  Hook erlaubt jetzt `git merge --ff-only` im eigenen Worktree (F14, Default A).
- M0-04 Recherche: 10 Themen mit `NOTIZEN.md` (Quellen, Stand 08.10.2026). Wichtigste Befunde: **JUCE 9.0.3 ist
  erschienen** (Auftrag §5 veraltet); Tracktion `develop` = 3.5.0 ungetaggt mit LUFS/Render-Queue/Mehrkanal; MCP-Spec
  2026-07-28 (zustandslos), kein C++-SDK; Claude-API max. 20 strict-Tools, `tool_choice any/tool` auf Opus/Sonnet 5.5 verboten;
  AGPLv3 bestätigt, Namenstreffer 2Simple „Tracklab“; JUCE-ALSA ohne RT-Thread → JACK über PipeWire; kein CLAP in JUCE 9;
  Screenshots laufen ohne Display. Brief-Vorschläge in M0-06 und M0-07. Entscheidungsrunde 2 (F16–F38).

## Wartet auf den PO

- **F0b** Vault-Zugriff (Obsidian-MCP fehlt in Cloud-Sitzungen) und Vault-Sync einrichten.
- **Entscheidungsrunde 1:** F1–F13, dazu F14 (Hook ↔ Vorlage) und F15 (Präfix `V`).
- **Entscheidungsrunde 2:** F16–F38 aus der Recherche (MCP, Claude-API, Lizenz, JUCE 9, Marke, Tracktion-Stand, Lautheit,
  EBU-Quellen, Signing, Plattformen, Songgrenzen, Plugin-Sandbox, CLAP/LV2, Linux-Audio, Barrierefreiheit, Docking/Theme).

## Nächster Schritt (Lead)

1. Antworten aus `TODO-PO.md` nach `ENTSCHEIDUNGEN.md`; DESIGN Rev 2 mit den Recherche-Korrekturen (JUCE 9, Tracktion 3.5,
   MCP 2026-07-28, strict-Limit, `tool_choice`, Linux-Audio, Screenreader, Sandbox-Stufen); Auftrag-Korrekturen als Liste
   für den PO; Präfix-Regel (F15) in `team/README.md`.
2. Nach F2/F3/F24/F26: ADR-001 (M0-05), dann M0-06 Engine-Spike starten (Brief-Vorschlag steht), danach M0-07 Gate.
3. M0-08: `daw-features` (nach F11), `vault-kontext` (nach F0b).
4. Mit Vault-Zugriff: Vault lesen, `Projektinhalt.md` abgleichen, Vault-Abschnitte *Status*/*Offene Punkte*/*Entscheidungen* patchen.

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | – |
| Review | – |
| Backlog | M0-05 ADR-001 · M0-06 Engine-Spike (Brief-Vorschlag steht) · M0-07 Gate CMake (Struktur-Vorschlag steht) · M0-08 Rest-Recherche |
| Erledigt | M0-01 · M0-02 · M0-03 · M0-04 |

## Builds

Noch keine.
