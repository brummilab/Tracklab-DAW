# Tracklab – Design

Die Spezifikation. Jede Änderung am Soll-Verhalten bekommt eine neue **Revision** (Rev n) mit Eintrag im
Änderungslog. Briefs verweisen auf Revision und Abschnitt.

## Änderungslog

| Rev | Datum | Inhalt | Entscheidungen |
|---|---|---|---|
| 1 | 08.10.2026 | Kurzfassung von §4–§12 des Auftrags (`docs/auftrag/Claude-Code-Prompt.md`) | – (Entscheidungsrunde 1 offen) |

Bei Widersprüchen gilt der Auftrag; jede spätere Revision verweist auf E-Einträge in
`team/ENTSCHEIDUNGEN.md`. Mit `[VERIFIZIEREN]` markierte Punkte werden in `team/research/` geklärt.

---

## 1. Produkt (Auftrag §4)
- Schnelle, stabile, schöne DAW für Live-Mitschnitte, Band-Recording, Editing, Mixing, Mastering;
  Windows 10/11 + Linux Mint/Ubuntu. Für Reaper-Nutzer sofort vertraut.
- Jede Aktion gleichwertig per Maus, Tastatur, Controller, Claude-Panel, MCP-Client. Ohne Claude voll nutzbar.
- **Nicht-Ziele:** Videoschnitt, macOS/AU/AAX, Notation auf Dorico-Niveau, Live-Performance-Fokus im MVP,
  Cloud-Kollaboration, eigene KI-Stem-Separation im MVP.
- **Akzeptanztests:** Workflow A (Live-Mitschnitt → Songs → −14 LUFS/−1 dBTP → WAV 48/24 + MP3 mit
  Messbericht) zuerst; Workflow B (12+ Kanäle, Comping, Mix, Mastering, Export) als Ausbau.

## 2. Tech-Stack – Vorschlag ADR-001 (Auftrag §5, nicht entschieden)
| Baustein | Vorschlag |
|---|---|
| Sprache/Build | C++20, CMake ≥ 3.25, Ninja; MSVC, Clang, GCC |
| Framework | JUCE 8 (AGPLv3 oder kommerziell) `[VERIFIZIEREN: aktuelle Version]` |
| Engine | Tracktion Engine v3.x (GPLv3 oder kommerziell) `[VERIFIZIEREN]` |
| Plugins | VST3 (MVP); LV2 (Linux) und CLAP (v1) |
| Audio | Windows ASIO + WASAPI; Linux ALSA + JACK-API (unter PipeWire über pipewire-jack) |
| GUI | native JUCE-Komponenten + Design-Tokens; WebView nur optional fürs Claude-Panel |
| HTTP | libcurl (SSE) oder JUCE `WebInputStream` – per ADR |
| JSON | nlohmann/json + Schema-Validierung |
| Tests | Catch2 oder GoogleTest; CTest |
| Keychain | Windows Credential Manager; libsecret |

Lizenzfolge: JUCE AGPLv3 + Tracktion GPLv3 → Tracklab unter **AGPLv3**. Alternativen (Ardour-Fork,
Rust-Stack, JUCE ohne Tracktion) werden in ADR-001 bewertet. **Pflicht vor Annahme:** Engine-Spike
(Import WAV/MP3, Region offline rendern + LUFS messen, 12 Eingänge mit Dummy-Device, VST3 laden,
CI-Build Windows + Linux) → `team/research/engine-spike/`.

## 3. Architektur (Auftrag §6)
**Module:** `core` (Modell, IDs, Undo, Command-Registry, Event-Bus) · `engine` (Tracktion-Adapter,
Transport, Aufnahme, Render, PDC) · `io` (Geräte, Backends, Latenzmessung) · `plugins` (Scanner im
eigenen Prozess, Hosting, Sandbox, Presets) · `dsp` (Meter, LUFS, Dither, Songgrenzen) · `project`
(Format, Autosave, Backups, Import/Export) · `ui` (Views, Tokens, Shortcuts, Screensets) ·
`assistant` (Claude-Client, Tools, Turn-Transaktionen, Kosten) · `mcp` (Streamable HTTP + stdio-Shim) ·
`cli` (`tracklab-cli`).

**Command-Registry (Herzstück):** jede Aktion = `Command` mit `id` (`track.create`), Titel (DE),
Beschreibung (EN), JSON-Schema (`additionalProperties: false`), Rückgabe-Schema, Flags
`readOnly|undoable|destructive|longRunning`, Shortcut, Menüpfad. Alle Eingabewege rufen dieselbe
Registry. Makros sind Command-Listen und selbst Commands. Commands laufen auf dem Message-Thread.
Registry erzeugt `docs/commands.md` und `tools.json`; CI prüft Aktualität.

**Echtzeit:** `docs/realtime.md`.

**Projektformat:** Tracktion-Edit (ValueTree/XML); Ordner `<Projekt>/<Projekt>.<ext>` + `Audio/`,
`Renders/`, `Backups/`, `Peaks/`, `claude-log.jsonl`. Autosave, rotierende Backups, Crash-Recovery,
relative Pfade, Formatversion + Migrationstests.

**Plugin-Hosting:** Scanner-Prozess, Blacklist, Cache; Sandbox-Modi nach Bitwig-Vorbild; Absturz →
Bypass + Markierung + „neu laden“, Projekt bleibt offen; PDC; FX-Chains/Container.

## 4. Claude-Integration (Auftrag §7)
- **Panel:** andockbar, SSE-Streaming, Abbrechen; Modi *Fragen* / *Automatisch* / *Nur vorschlagen*;
  Plan-Vorschau über `assistant.propose_plan`; Kontext-Chips; Verlauf + Audit-Log `claude-log.jsonl`.
- **Sicherheit:** Turn = Undo-Transaktion; `destructive` immer bestätigen; max. 50 Commands/Turn;
  keine Shell, nur freigegebene Ordner.
- **API:** REST `POST /v1/messages`, `anthropic-version: 2023-06-01`, SSE. Tools automatisch aus der
  Registry, `strict: true`; Tool Search (`defer_loading`) mit 3–5 Kern-Tools nicht deferred; Prompt
  Caching auf System-Prompt + letztes nicht-deferred Tool; Effort-Parameter.
  `[VERIFIZIEREN: Tool-Namensmuster, Tool-Search-Version, Effort, Haiku + Tool Search]`
- **Analyse-Tools:** `project.get_state`, `track.list/get`, `mixer.get_routing`, `selection.get/set`,
  `analyze.loudness|find_song_boundaries|spectrum|peaks|clipping|phase|compare_reference|verify_change`.
  Claude „hört“ nur über Messwerte; das Panel zeigt sie an.
- **Modelle:** Standard `claude-sonnet-5-5`; `claude-opus-5-5` für Analyse/Planung; `claude-haiku-5-5`
  für Ein-Schritt-Befehle. Kosten aus `usage`, Preise in `presets/pricing.json`.
- **Key:** nur im OS-Schlüsselbund; offline → Panel „offline“, Rest läuft.
- **MCP-Server:** opt-in, nur `127.0.0.1`, Bearer-Token, Origin-Prüfung, Rate-Limit, Audit-Log;
  Streamable HTTP + stdio-Shim `tracklab-mcp`; eigene schlanke JSON-RPC-Implementierung mit
  Conformance-Tests (Alternative: Rust-Shim mit `rmcp`). Dieselbe Registry, dieselben Regeln.
  Keine freie Skriptausführung. `[VERIFIZIEREN: MCP-Spec-Revision, CLI-Syntax]`
- **Akzeptanzbefehle:** Tabelle §7.8 des Auftrags → `tests/assistant/`.

## 5. GUI/UX (Auftrag §8)
- Views: Arrange (Lanes), Mixer, Editor (Audio, Piano-Roll, Drum), Browser, Inspector, Transport,
  Region-Manager, Mastering-Seite, Claude-Panel, Render-Queue.
- Design-Tokens in `themes/*.json`; Dark (Standard) + Light ohne Neustart; HiDPI 100–300 %, SVG-Icons.
- **Branding-Tokens** (`assets/branding/README.md`): Akzent `#F59E0B` (Playhead, Auswahl), Clips
  `#38BDF8` / `#2DD4BF` / `#818CF8`, Hintergrund `#0F172A`–`#1E2A47`, Text hell `#E5E7EB`.
  Windows-Icon `tracklab.ico`, Linux `png/tracklab-icon-256.png`/`-512.png`. Logo in README, Splash,
  About. Logo-Änderungen nur per Entscheidung, PNG/ICO nur über `render-icons.py`.
- Screensets, Shortcut-Presets (Tracklab, Reaper-kompatibel, später Pro Tools/Cubase), Maus-Modifier,
  Action-Liste mit Suche, Tastaturbedienung, Kontrastmodus.
- Screenshot-Prüfung: `tracklab-cli screenshot …` offscreen (Linux-CI mit Xvfb), Vorher/Nachher je
  GUI-Karte unter `docs/screenshots/<karte>/`.

## 6. Feature-Katalog (Auftrag §9)
Vollständige Tabellen mit Herkunft, Priorität (MVP/v1/v2/später) und Tool-Namen: Auftrag §9.1–§9.17.
MVP-Schwerpunkte: Projekt/Autosave, Spurtypen + Ordner + Gruppen, Lanes, 12-Kanal-Aufnahme, Monitoring,
Punch, Loop-Takes, Latenzmessung, Split/Trim/Fades, Dynamic Split, Razor, Swipe-Comping, Clip-Gain,
Marker/Regionen + Songgrenzen, Tempo-Spur, Konsole/Inserts/Sends/Busse, PDC, Automations-Lanes,
VST3-Hosting, Basis-FX (EQ, Komp, Gate, TP-Limiter), LUFS-Meter, Dither, Render-Dialog/Queue/pro Region,
Stems, Wildcards, LUFS-Normalisierung, Audio-Import, Action-Liste/Makros, Shortcut-Presets, Themes,
CPU-Anzeige, agentischer Claude-Assistent. Mit `†` markierte Herkünfte vor Umsetzung belegen.

## 7. Mastering & Export (Auftrag §10)
- Formate MVP: WAV/BWF, FLAC, MP3 (Encoder-Lizenz per ADR); v1: AIFF, Ogg, DAWproject, MIDI.
- Lautheits-Presets: Video-Pipeline −14 LUFS/−1 dBTP (WAV 48/24), Spotify, Apple, YouTube, Amazon,
  Tidal, Deezer, EBU R128, „nur messen“ (Standard). `[VERIFIZIEREN vor Release]`
- Render-Pipeline: Master-Kette → Normalisierung (messen → Gain → TP-Limiter) → SRC → Dither (nur bei
  Bittiefen-Reduktion, letzte Stufe) → Encoder → Metadaten → Prüfmessung, Bericht `.json` neben der Datei.

## 8. Qualität, CI, Release (Auftrag §11)
- Tests zuerst (test-writer): Unit, Engine-Integration, Registry-Vertrag, Assistant-Simulation ohne Netz,
  MCP-Conformance, GUI-Screenshots.
- Golden-Render: `tests/fixtures/mitschnitt-mini`, `band-mini`; Null-Test < −90 dBFS; Updates nur per Karte.
- LUFS-Validierung gegen EBU Tech 3341/3342 (Signale per CI-Download, nicht im Repo).
- `tracklab-cli`: `render`, `analyze`, `find-songs`, `run-commands`, `screenshot`, `validate-lufs`, `export-tools`.
- CI-Jobs: build-test-linux (GCC + Clang, RTSan), build-test-windows (MSVC), package-linux (AppImage,
  .deb), package-windows (Installer + ZIP), smoke, docs-check. Artefakte bei jedem Push auf `main`.
- Release: Freigabe durch David → Tag `vJJJJ.MM.N` (in Cloud-Sessions setzt David den Tag).

### Gate (DoD je Karte)
CMake Release + Debug ohne Warnungen in `src/` (`-Werror`) · ctest grün · Golden + LUFS grün ·
RTSan ohne Befund · clang-format/clang-tidy sauber · Secret-Scan sauber · nur Brief-Dateien geändert ·
Doku nachgezogen (DESIGN, README, `Projektinhalt.md`, `docs/commands.md`) · reviewer `APPROVE` ·
GUI-Screenshots. **Stand Rev 1:** `scripts/gate.sh`/`.ps1` prüfen Pflichtdateien, CLAUDE.md-Länge,
Secrets, Audio-Dateien, `core.hooksPath`, clang-format; Build-Schritte folgen mit dem Spike (M0-07).

## 9. Roadmap (Auftrag §12)
| M | Ziel |
|---|---|
| M0 | Vorlage, Gate auf C++, Agents, Recherche, ADR-001, Engine-Spike |
| M1 | Fundament: CMake, JUCE + Tracktion, Audio-I/O, Projektformat, Autosave, Registry v1, Undo, CLI, CI |
| M2 | Mitschnitt-Workflow ohne Claude (Workflow A von Hand) |
| M3 | Claude-Integration MVP + MCP-Server (Workflow A per Claude und per Claude Code) |
| M4 | Spuren & Aufnahme (12 Kanäle) |
| M5 | Editing & Comping |
| M6 | Mixer & Plugins (VST3, Sandbox, Basis-FX) |
| M7 | Automation & MIDI-Basis |
| M8 | Mastering & Export komplett |
| M9 = 1.0 | Interop & Politur (DAWproject, LV2, CLAP, Reaper-Preset, Installer) |

Optimierungsrunden nach M2, M5, M8.
