# Tracklab

> Repo-Fassung der Projektnotiz. Führend ist der Vault: `Tracklab/Projektinhalt.md` (R2).
> In Session 1 war der Vault aus der Cloud-Session nicht erreichbar; diese Datei wurde aus dem
> Auftrag (`docs/auftrag/Claude-Code-Prompt.md`) aufgebaut und wird beim nächsten Vault-Zugriff
> mit der Vault-Notiz abgeglichen. Den Abschnitt `## Verlauf` führt nur der Vault (Git-Hook, R6).

## Ziel
Eigene DAW für Windows 10/11 und Linux (Mint/Ubuntu) für Live-Mitschnitte, Band-Recording,
Editing, Mixing und Mastering. Claude ist integriert (Assistant-Panel, Tools aus der
Command-Registry) und Tracklab ist per lokalem MCP-Server von Claude Desktop/Claude Code steuerbar.
Ohne Claude voll nutzbar.

## Eckdaten
| | |
|---|---|
| Product Owner | David |
| Repo | `brummilab/Tracklab-DAW` (privat), Branch `main` |
| Arbeitsweise | Agent-Team-Vorlage (Team Lead + Sub-Agents), Loop aus Auftrag §3.2 |
| Vorlage | `agent-team-vorlage` **v1.0.0**, übernommen am 08.10.2026 (per ZIP vom PO) |
| Stack (Vorschlag ADR-001) | C++20, CMake, JUCE 8, Tracktion Engine 3.x, VST3 (MVP), LV2/CLAP (v1) |
| Lizenz (Vorschlag) | AGPLv3 |
| Branding | `assets/branding/` (Logo, Icon, Farben; Akzent `#F59E0B`) |
| Release-Schema | `vJJJJ.MM.N` (zu bestätigen) |

## Referenz-Workflows
- **A – Live-Mitschnitt:** Import, Songgrenzen per Setlist, Trims/Fades, −14 LUFS / −1 dBTP,
  Export pro Song als WAV 48/24 (DaVinci Resolve) + MP3 mit Messbericht. *Zuerst umsetzen.*
- **B – Multitrack-Band:** 12+ Eingänge, Comping in Lanes, Drum-Bus/Parallelkompression,
  Mastering, Export WAV/FLAC/MP3/Stems.

## Roadmap
M0 Start/Spike · M1 Fundament · M2 Mitschnitt ohne Claude · M3 Claude-Integration + MCP ·
M4 Spuren & Aufnahme · M5 Editing & Comping · M6 Mixer & Plugins · M7 Automation & MIDI ·
M8 Mastering & Export · M9 = 1.0 Interop & Politur. Optimierungsrunden nach M2, M5, M8.

## Status
- 08.10.2026: M0 begonnen. Grundgerüst (CLAUDE.md, README, Gate, CI, Hook `guard-git.sh`),
  Branding nach `assets/branding/`, Design Rev 1, Recherche-Themen, Entscheidungsrunde 1 angelegt.
- 08.10.2026: Agent-Team-Vorlage v1.0.0 übernommen (M0-03): Prozessvertrag, 6 Sub-Agents (inkl. `implementer-rt`
  für Echtzeit-Code), Board als Dateien, Plan; Gate prüft zusätzlich die Agent-Definitionen.
- 08.10.2026: Recherche M0-04 abgeschlossen (10 Themen, `team/research/*/NOTIZEN.md`). Kernbefunde: JUCE 9.0.3 ist
  erschienen, Tracktion Engine `develop` 3.5.0 bringt LUFS-Messung und Render-Queue, MCP-Spec 2026-07-28, AGPLv3
  bestätigt, Namenstreffer 2Simple „Tracklab“. Brief-Vorschläge für Engine-Spike (M0-06) und Gate (M0-07).
- Blockiert: Entscheidungsrunden 1 (F1–F15) und 2 (F16–F38) sowie Vault-Zugriff (`team/TODO-PO.md`).

## Offene Punkte
- F0b Vault-Zugriff in Cloud-Sessions (Obsidian-MCP fehlt) bzw. Vault-Sync per post-merge-Hook einrichten.
- Entscheidungsrunde 1 (F1–F13) sowie Widersprüche Vorlage ↔ Auftrag (F14 Hook/merge, F15 Präfix `V`).
- Entscheidungsrunde 2 (F16–F38) aus der Recherche.
- Rest-Recherche M0-08: DAW-Funktionen (nach F11), Vault-Kontext (nach F0b).
- Engine-Spike (JUCE + Tracktion Engine) nach Annahme von ADR-001.

## Entscheidungen
- E0 (08.10.2026): Arbeitsweise mit Agent-Team – ja; Vorlage v1.0.0 übernommen.
