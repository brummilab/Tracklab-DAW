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

Vision (E53): eigenständige DAW mit dem Funktionsumfang der großen DAWs (schrittweise, nach 1.0 der restliche
Feature-Katalog). Alleinstellung: Claude + MCP in jeder Aktion, Live-Mitschnitt-Automatik, Open Source unter Windows
und Linux, eigene Plugins/DSP.

## Eckdaten
| | |
|---|---|
| Product Owner | David |
| Repo | `brummilab/Tracklab-DAW` (öffentlich seit 09.10.2026, E45), Branch `main` |
| Arbeitsweise | Agent-Team-Vorlage (Team Lead + Sub-Agents), Loop aus Auftrag §3.2 |
| Vorlage | `agent-team-vorlage` (privat) **v1.0.0**, übernommen am 08.10.2026 (per ZIP vom PO) |
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
M0 Start/Spike · M1 Fundament · M2 Workflow A Live-Mitschnitt (Stille-Erkennung, Setlist, LUFS, Region-Export mit BWF-Zeitstempel) · M3 Claude-Integration + MCP ·
M4 Spuren & Aufnahme · M5 Editing & Comping · M6 Mixer & Plugins inkl. Cue-Mixe · M7 Automation & MIDI ·
M8 Mastering & Export inkl. M/S · M9 = 1.0 Interop & Politur inkl. Handy-Fernbedienung · ab M10 restlicher Feature-Katalog (E53). Optimierungsrunden nach M2, M5, M8.

## Status
- 08.10.2026: M0 begonnen. Grundgerüst (CLAUDE.md, README, Gate, CI, Hook `guard-git.sh`),
  Branding nach `assets/branding/`, Design Rev 1, Recherche-Themen, Entscheidungsrunde 1 angelegt.
- 08.10.2026: Agent-Team-Vorlage v1.0.0 übernommen (M0-03): Prozessvertrag, 6 Sub-Agents (inkl. `implementer-rt`
  für Echtzeit-Code), Board als Dateien, Plan; Gate prüft zusätzlich die Agent-Definitionen.
- 08.10.2026: Recherche M0-04 abgeschlossen (10 Themen, `team/research/*/NOTIZEN.md`). Kernbefunde: JUCE 9.0.3 ist
  erschienen, Tracktion Engine `develop` 3.5.0 bringt LUFS-Messung und Render-Queue, MCP-Spec 2026-07-28, AGPLv3
  bestätigt, Namenstreffer 2Simple „Tracklab“. Brief-Vorschläge für Engine-Spike (M0-06) und Gate (M0-07).
- 08.10.2026: Entscheidungsrunden 1 und 2 mit „Defaults ok“ entschieden (E1–E38), Lizenz AGPLv3, Design Rev 2,
  ADR-001-Entwurf (JUCE 9 + Tracktion 3.5 unter Spike-Vorbehalt).
- 08.10.2026: Engine-Spike M0-06 gemerged – JUCE 9.0.3 + Tracktion `develop` bauen zusammen; Import WAV/MP3, Render +
  Lautheit, 12-Kanal-Aufnahme, VST3 laufen headless (57/57 Tests, Linux). Windows-CI grün, MP3 plattformgleich.
- 08.10.2026: ADR-001 angenommen (E39). Gate auf C++/CMake umgebaut (M0-07): Build GCC/Clang/MSVC, clang-format/-tidy,
  RealtimeSanitizer (Clang 20) mit Negativtest.
- 08.10.2026: **M0 abgeschlossen** – CI grün (Linux GCC/Clang, Windows MSVC, RTSan). GitHub-About mit Beschreibung und
  Topics gesetzt. Rückblick `team/RUECKBLICK-M0.md`. Nächster Meilenstein: M1 Fundament.
- 08.10.2026: M1 geplant (9 Karten, Design Rev 3, E40–E42). M1-01 erledigt: Projektgerüst `src/`, gemeinsame
  JUCE/Tracktion-Bibliothek unter `third_party/`, Engine-Fabrik mit atomaren Einstellungen.
- 08.10.2026: GitHub-Actions-Minuten aufgebraucht → CI sparsam (E44): nur `main`, nur bei Code-Änderung, Debug-Beine +
  RTSan; volle Matrix manuell. M1-03/M1-06 auf PO-Wunsch angehalten.
- 08.10.2026: Umstieg auf öffentliches Repo beschlossen (E45); Prüfung ohne Secrets-/Audio-Funde, Restfragen F43.
- 08.10.2026: F43 mit Defaults entschieden (E46): Vorlage-Nennung gekürzt, Release-Beine wieder bei jedem Code-Push,
  Markenprüfung „Tracklab“ vor dem ersten Release (O-07). Private Pfade und Namen anderer Projekte neutralisiert.
- 08.10.2026: README neu auf das Produkt ausgerichtet (Funktionen, Claude-Integration, Stand, Selbst bauen), ohne
  Bezug auf andere DAWs (PO-Wunsch).
- 09.10.2026: Git-Historie umgeschrieben (E47) – private Pfade und Namen aus allen Commits entfernt, Force-Push.
  README-Logo passt sich dem GitHub-Theme an.
- 09.10.2026: Repo öffentlich (E45) – Actions-Minuten für Standard-Runner unbegrenzt.
- 09.10.2026: O-08 – Windows-CI war seit M1-01 rot (Link-Fehler), behoben; CI auf `main` komplett grün.
- 09.10.2026: **M1-03 Undo/Redo gemerged** – Transaktionen über die Registry (ein Befehl bzw. Batch = ein Undo-Schritt),
  Rollback bei Fehlern, `edit.undo`/`edit.redo`/`edit.get_undo_state`, 200 Stufen, Vertragstest für alle undoable Commands.
  Bekannte Einschränkung F44 (JUCE-Stash) → O-09.
- 09.10.2026: **M1-06 Audio-Geräte gemerged** – `io.list_device_types`/`list_devices`/`get_device`/`set_device` für ALSA,
  JACK/PipeWire, WASAPI, ASIO; Persistenz; belegte Geräte → klarer Fehler statt stummem Ausgang. Windows-CI grün.
  Handtest V1 nach `docs/testing/manual/M1.md` (David). Folgekarte O-10.
- 09.10.2026: O-11 (Windows-CI: lame/ffmpeg mit Retry + Cache) erledigt. O-09 Teil B: Befehlspakete werden vor dem ersten
  Schritt vollständig geprüft – ungültige Pakete ändern nichts, Undo-Historie bleibt. Teil C (JUCE-Patch) wartet auf F46.
- 09.10.2026: O-09 komplett – JUCE-Undo-Patch (E49) wird beim Bauen automatisch angewendet; gescheiterte Befehle
  verlieren keine Undo-Historie mehr (F44 erledigt). GUI-Leitbild E50: modern, aufgeräumt, dunkel. Eigene Plugins E48.

## Offene Punkte
- Infos ohne Default: F9 Audio-Interface, F10 Reaper-MCP, F11 `reaper-kb.ini`, F12 lokal/Cloud, F22 Test-API-Key.
- [ ] Stem-Separation (v2): Library und Lizenz klären (eigenes ADR).
- Reaper-Preset (M0-08) nach F11; Risiko Mint 23 / libstdc++ 15 (O-03).

## Entscheidungen
- E54 (09.10.2026): Auftrag ergänzt (Stille-Erkennung, Setlist-Import, BWF-Zeitstempel, Cue-Mixe v1, M/S v1,
  Handy-Fernbedienung v1, Stem-Separation v2); Workflow A als M2 mit Mojo-Club-Abnahme; Claude-Integration bleibt M3.
- E53 (09.10.2026): Vision bestätigt – Funktionsumfang der großen DAWs schrittweise, Nicht-Ziele bleiben;
  Alleinstellung Claude + MCP, Live-Mitschnitt-Automatik, Open Source Win/Linux, eigene Plugins/DSP.
- E52 (09.10.2026): Kein Reverse Engineering fremder DAWs; Vergleich nur über öffentlich Dokumentiertes.
- E43 (08.10.2026): Bei C++ bleiben (kein Rust-Umstieg); Rust höchstens später für abgegrenzte Prozesse.
- E40–E42 (08.10.2026): Projektdatei `.tracklab`, Autosave 2 min + 10 Backups, Namen Tracklab/tracklab-cli/tracklab-mcp.
- E39 (08.10.2026): ADR-001 angenommen – JUCE 9.0.3, Tracktion Engine develop, AGPLv3, doctest.
- E0b (08.10.2026): Vault-Zugriff aus Cloud-Sessions nicht nötig – Obsidian Git Sync aktiv.
- E1–E38 (08.10.2026): Defaults übernommen – u. a. Lizenz AGPLv3, Stack unter Spike-Vorbehalt, Workflow A zuerst,
  Sonnet 5.5 als Standardmodell, Inno Setup, Release per Tag nach Freigabe, MCP 2026-07-28 mit eigener C++-Implementierung,
  JACK über PipeWire unter Linux, Plugin-Sandbox nach Latenz-Spike. Details `team/ENTSCHEIDUNGEN.md`.
- E0 (08.10.2026): Arbeitsweise mit Agent-Team – ja; Vorlage v1.0.0 übernommen.
