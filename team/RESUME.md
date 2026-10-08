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

## Wartet auf den PO

- **F0b** Vault-Zugriff (Obsidian-MCP fehlt in Cloud-Sitzungen) und Vault-Sync einrichten.
- **Entscheidungsrunde 1:** F1–F13, dazu F14 (Hook ↔ Vorlage) und F15 (Präfix `V`).
- **Entscheidungsrunde 2:** F16–F30 aus der Recherche (MCP, Claude-API, Lizenz, JUCE 9, Marke, Tracktion-Stand, Lautheit, EBU-Quellen, Signing, Plattformen).

## Nächster Schritt (Lead)

1. Antworten aus `TODO-PO.md` nach `ENTSCHEIDUNGEN.md`, DESIGN Rev 2, Präfix-Regel (F15) in `team/README.md`.
2. M0-04 Recherche starten: `researcher` je Thema, max. 4 parallel; zuerst `engine-spike`, `lizenz-und-name`,
   `claude-api`, `mcp-server` (geht auch ohne Antworten).
3. Mit Vault-Zugriff: Vault lesen, `research/vault-kontext/README.md` füllen, `Projektinhalt.md` abgleichen,
   Vault-Abschnitte *Status*/*Offene Punkte*/*Entscheidungen* patchen.
4. Nach F2/F3 + Recherche: ADR-001 (M0-05), Brief für den Engine-Spike (M0-06), Gate auf CMake (M0-07).

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | M0-04 Recherche – fertig: mcp-server, claude-api, lizenz-und-name, engine-spike, loudness-standards, ci-packaging; läuft: audio-backends-linux, plugin-hosting-clap-lv2, gui-design-system, songgrenzen-erkennung; offen: daw-features (nach F11), vault-kontext (nach F0b) |
| Review | – |
| Backlog | M0-05 ADR-001 · M0-06 Engine-Spike (Brief-Vorschlag steht) · M0-07 Gate CMake (Struktur-Vorschlag steht) |
| Erledigt | M0-01 · M0-02 · M0-03 |

## Builds

Noch keine.
