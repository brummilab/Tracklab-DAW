# Tracklab – Design

Die Spezifikation. Jede Änderung am Soll-Verhalten bekommt eine neue **Revision** (Rev n) mit Eintrag im
Änderungslog. Briefs verweisen auf Revision und Abschnitt.

## Änderungslog

| Rev | Datum | Inhalt | Entscheidungen |
|---|---|---|---|
| 3 | 08.10.2026 | M1-Fundament: Projektformat, Speichern/Autosave, Undo, Registry v1, Headless, Settings/Geräte (`team/research/m1-fundament/NOTIZEN.md`) | E40–E42 |
| 2 | 08.10.2026 | Recherche M0-04 eingearbeitet: JUCE 9, Tracktion 3.5, MCP 2026-07-28, Claude-API-Grenzen, Linux-Audio, Plugin-Sandbox-Stufen, GUI/Barrierefreiheit, Songgrenzen, CI | E1–E8, E13–E21, E23–E38 |
| 1 | 08.10.2026 | Kurzfassung von §4–§12 des Auftrags (`docs/auftrag/Claude-Code-Prompt.md`) | – (Entscheidungsrunde 1 offen) |

Bei Widersprüchen gilt der Auftrag, außer eine Revision korrigiert ihn ausdrücklich (Liste „Korrekturen am Auftrag“ unten); jede spätere Revision verweist auf E-Einträge in
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

## 2. Tech-Stack – ADR-001 (angenommen, E39)
`docs/adr/ADR-001-tech-stack.md`. Belege: `team/research/*/NOTIZEN.md`.

| Baustein | Festlegung (Rev 2) |
|---|---|
| Sprache/Build | C++20, CMake ≥ 3.25, Ninja; MSVC (Windows), GCC + Clang (Linux); nur x64 (E30) |
| Framework | **JUCE 9.0.3** (`be29c81`) – Spike bestanden (E24, E39). Git-Submodule, `add_subdirectory` |
| Engine | **Tracktion Engine `develop` @ `bb38617`** (3.5.0, ungetaggt; E26, E39); Upgrade nur per Karte |
| Plugins | VST3 (MVP, SDK 3.8 MIT in JUCE); LV2 über JUCE (nur X11-UIs, E35); CLAP eigener Adapter (clap + clap-helpers, v1) |
| Audio | Windows ASIO (GPLv3-Option des SDK) + WASAPI exklusiv/geteilt; Linux **JACK-API über pipewire-jack** als Standard, Start über `pw-jack` (E36), ALSA Zweitoption |
| GUI | native JUCE-Komponenten + Design-Tokens; Windows Direct2D, Linux Software-Renderer (X11/XWayland) |
| HTTP | **libcurl direkt** (curl_multi im Worker-Thread) für SSE (E21) |
| JSON | nlohmann/json + Schema-Validierung |
| Tests | doctest 2.4.11 aus dem Tracktion-Pin + CTest (Spike M0-06) |
| MP3 | Dekoder JUCE `MP3AudioFormat` (`JUCE_USE_MP3AUDIOFORMAT=1`, `JUCE_USE_WINDOWS_MEDIA_FORMAT=0`); Encoder per späterem ADR (LAME extern/LibLame/FFmpeg) |
| Keychain | Windows Credential Manager; libsecret |

**Lizenz (E2):** AGPLv3 (`AGPL-3.0-only`), `LICENSE` ersetzt. Builds an Bandmitglieder = Weitergabe → Quelltext geht
mit; „Über Tracklab“ zeigt Lizenz und Quelltext-Link (E23, M1). Marke: privat weiter, Prüfung vor Veröffentlichung (E25).

**Engine-Spike (M0-06, Pflicht vor endgültiger Annahme):** Import WAV/MP3, Region offline rendern + LUFS/True Peak/LRA
messen, 12 Eingänge über `HostedAudioDeviceInterface`, eigenes Test-VST3 laden, CI-Build Windows + Linux →
`team/research/engine-spike/BERICHT.md`.

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

**Registry v1 (Rev 3):** `Command{id, titleDe, descriptionEn, paramsSchema, resultSchema, flags, shortcut, menuPath,
handler}`; Parameter werden **immer** lokal gegen das Schema validiert (nlohmann/json 3.12.0 + pboettch/json-schema-validator
2.4.0, Draft 7). Schemas nur im Subset `type, properties, required, additionalProperties:false, enum, minimum/maximum,
items, anyOf, $ref/$defs` – ein Registry-Test erzwingt das. Tool-Name = ID mit `_` statt `.`; Map in beide Richtungen,
Kollision → Startfehler. Fehler als strukturierte Antwort `{ok:false, error:{code, message, pointer}}`.
`tracklab-cli export-tools` erzeugt `tools.json` und `docs/commands.md`; das Gate prüft Aktualität.

**Undo (Rev 3):** Alle `undoable`-Commands schreiben über `Edit::getUndoManager()`. Ausführungsrahmen der Registry:
`UndoTransactionInhibitor` → `beginNewTransaction(name)` → Command(s) → freigeben. Eine Geste/ein Makro/ein Claude-Turn =
eine Transaktion. Undo-Tiefe 200 (einstellbar). Vertragstest je Command: Undo stellt den Zustand wieder her (normalisiert),
Redo wiederholt ihn.

**Engine-Fabrik (Rev 3):** eine Stelle erzeugt `tracktion::Engine` mit eigener `PropertyStorage` (Settings-Datei unter
`userApplicationDataDirectory/Tracklab/`, atomar; Tests/CLI in-memory), eigenem `UIBehaviour` (headless ohne Dialoge),
`EngineBehaviour` (Geräte nur in der App), `getUserName()` = `"Tracklab"`, `getApplicationVersion()` = Tracklab-Version.

**Echtzeit:** `docs/realtime.md`.

**Projektformat (Rev 3, E40/E41):** Tracktion-`Edit` (ValueTree/XML) **ohne** Tracktion-`Project`/`ProjectManager`;
Ordner `<Projekt>/<Projekt>.tracklab` + `Audio/`, `Renders/`, `Backups/`, `Peaks/`, `claude-log.jsonl`.
`tracklabFormatVersion` am `EDIT`-Knoten, Migrationen auf dem ValueTree vor `Edit::createEdit`, je Version ein Test.
Relative Pfade (`alwaysUseRelativePaths`, Auflösung relativ zur Projektdatei); Rundlauftest mit verschobenem Ordner.
**Speichern atomar** (Temp-Datei im selben Ordner → flush → ersetzen), nie Tracktions `EditFileOperations::save`.
Backups: bei jedem Speichern rotierend 10 Versionen in `Backups/`. Autosave: alle 2 min (einstellbar) in
`<Projekt>.tracklab.autosave`, nur wenn geändert; beim Öffnen neuere Autosave → Wiederherstellen anbieten.
Unlesbare Projektdatei → Fehler mit Angebot der letzten Backup-Version. Keine Personendaten in der Datei.

**Plugin-Hosting (E34, E35):** Scanner im eigenen Prozess für **alle** Formate (Tracktion-Mechanismus erweitern, er
lagert heute nur VST/AU aus), Timeout je Datei, Blacklist + Cache (Änderungszeit/Version). Stufen: M6 = Scanner-Isolation
+ Absturzerkennung beim nächsten Start; volle Sandbox nach Bitwig-Modi (Within/Together/By manufacturer/By plug-in/
Individually) als eigenes Paket nach einem Latenz-Spike. Sandbox-Architektur: Binary `tracklab-plugin-host`, Steuerung
über Socket/Named Pipe, Audio/MIDI über Shared Memory, Handshake per Semaphor/Futex mit Timeout; Audio-Thread-Seite nur
Atomics und vorab gemappter, gesperrter Speicher; bei Timeout sofort Bypass + Markierung, Wiederherstellung im
Message-Thread. Kein Signal-Handler-Abfang im Audio-Thread. PDC; FX-Chains/Container.

## 4. Claude-Integration (Auftrag §7)
- **Panel:** andockbar, SSE-Streaming, Abbrechen; Modi *Fragen* / *Automatisch* / *Nur vorschlagen*;
  Plan-Vorschau über `assistant.propose_plan`; Kontext-Chips; Verlauf + Audit-Log `claude-log.jsonl`.
- **Sicherheit:** Turn = Undo-Transaktion; `destructive` immer bestätigen; max. 50 Commands/Turn;
  keine Shell, nur freigegebene Ordner.
- **API:** REST `POST /v1/messages`, `anthropic-version: 2023-06-01`, SSE über libcurl. Tools automatisch aus der
  Registry; Tool-Name = Command-ID mit `_` statt `.` (Muster `^[a-zA-Z0-9_-]{1,128}$`), Rückabbildung über eine Map,
  Kollisionsprüfung beim Start. **`strict: true` nur für Kern-Tools und destruktive Commands** (API-Limit 20 strict-Tools,
  24 optionale Parameter je Request), **immer** lokale Validierung gegen das Registry-Schema inkl. Wertebereiche (E19).
  Tool Search `tool_search_tool_bm25_20251119` mit `defer_loading`, 3–5 Kern-Tools nicht deferred; `cache_control` auf
  System-Prompt und das letzte **nicht-deferred** Tool (auf deferred Tools → HTTP 400); Tool-Liste je Sitzung stabil.
  Effort über `output_config.effort` (`low|medium|high|xhigh|max`). `message_delta.usage` ist kumulativ.
- **Plan-Vorschau (E20):** `tool_choice` `any`/`tool` ist auf Opus/Sonnet 5.5 verboten → `assistant.propose_plan` wird
  über den System-Prompt angefordert (`tool_choice: auto`).
- **Analyse-Tools:** `project.get_state`, `track.list/get`, `mixer.get_routing`, `selection.get/set`,
  `analyze.loudness|find_song_boundaries|spectrum|peaks|clipping|phase|compare_reference|verify_change`.
  Claude „hört“ nur über Messwerte; das Panel zeigt sie an.
- **Modelle (E5):** Standard `claude-sonnet-5-5`; `claude-opus-5-5` für Analyse/Planung; `claude-haiku-5-5` für
  Ein-Schritt-Befehle (unterstützt Tool Search); `claude-fable-5-1` nur auf ausdrücklichen Wunsch. Kosten aus `usage`,
  Preise in `presets/pricing.json` mit Preisstufen (Haiku 5.5: Schwelle 100.000 Prompt-Tokens).
- **Key:** nur im OS-Schlüsselbund; offline → Panel „offline“, Rest läuft.
- **MCP-Server (E16–E18):** opt-in, nur `127.0.0.1`, statisches Bearer-Token (Umgebung/Schlüsselbund, nie im Repo),
  **Origin- und Host-Prüfung** (403), Rate-Limit, Audit-Log. Spec **2026-07-28** primär (zustandslos, `_meta`,
  Header `Mcp-Method`/`Mcp-Name`), 2025-11-25 (`initialize`, `Mcp-Session-Id`) als Fallback. Eigene C++-JSON-RPC-
  Implementierung + C++-stdio-Shim `tracklab-mcp` (für Claude Desktop unter Windows); Linux-Clients: Claude Code
  (`claude mcp add --transport http tracklab http://127.0.0.1:<port>/mcp --header "Authorization: Bearer <token>"`).
  Offizielle Conformance-Suite `@modelcontextprotocol/conformance` (Version gepinnt) im Gate. Undo-Transaktion je
  `tools/call`. Dieselbe Registry, dieselben Regeln. Keine freie Skriptausführung.
- **Akzeptanzbefehle:** Tabelle §7.8 des Auftrags → `tests/assistant/`.

## 5. GUI/UX (Auftrag §8)
- **Leitbild (E50):** modern und aufgeräumt, Apple-inspiriert – viel Weißraum, ein Akzent (Bernstein), klare
  Typografie, feine Linien, dezente Schatten; jede Leiste nur mit dem Nötigsten.
- Views: Arrange (Lanes), Mixer, Editor (Audio, Piano-Roll, Drum), Browser, Inspector, Transport,
  Region-Manager, Mastering-Seite, Claude-Panel, Render-Queue.
- Design-Tokens in `themes/*.json`; Dark (Standard) + Light ohne Neustart; HiDPI 100–300 %, SVG-Icons.
- **Branding-Tokens** (`assets/branding/README.md`): Akzent `#F59E0B` (Playhead, Auswahl), Clips
  `#38BDF8` / `#2DD4BF` / `#818CF8`, Hintergrund `#0F172A`–`#1E2A47`, Text hell `#E5E7EB`.
  Windows-Icon `tracklab.ico`, Linux `png/tracklab-icon-256.png`/`-512.png`. Logo in README, Splash,
  About. Logo-Änderungen nur per Entscheidung, PNG/ICO nur über `render-icons.py`.
- Screensets, Shortcut-Presets (Tracklab, Reaper-kompatibel, später Pro Tools/Cubase), Maus-Modifier,
  Action-Liste mit Suche, Tastaturbedienung, Kontrastmodus.
- Screenshot-Prüfung: `tracklab-cli screenshot …` rendert Komponentenbäume per
  `createComponentSnapshot(…, SoftwareImageType())` – **ohne Display, ohne Xvfb** (geprüft); Xvfb nur als Rückfall.
  Vorher/Nachher je GUI-Karte unter `docs/screenshots/<karte>/`.
- **Tokens:** zwei Ebenen `primitive`/`semantic`, Overlays per `extends` (Light, High-Contrast, `cvd-safe`), Schema mit
  `additionalProperties: false`. Light-Theme mit abgedunkelten Akzenten (E38). Meter nie nur über Farbe.
- **Docking (E38):** Eigenbau (Split/Tab-Baum), Screensets als JSON, über die Registry steuerbar.
- **Barrierefreiheit (E37):** Screenreader nur Windows (UIA/Narrator, JUCE kann kein AT-SPI); Linux: Tastatur,
  Kontrast, Skalierung. Wayland nur über XWayland.
- **Schrift:** Inter (OFL) als Variable Font ungekürzt aus BinaryData; OFL-Text in die Third-Party-Notices.

## 6. Feature-Katalog (Auftrag §9)
Vollständige Tabellen mit Herkunft, Priorität (MVP/v1/v2/später) und Tool-Namen: Auftrag §9.1–§9.17.
MVP-Schwerpunkte: Projekt/Autosave, Spurtypen + Ordner + Gruppen, Lanes, 12-Kanal-Aufnahme, Monitoring,
Punch, Loop-Takes, Latenzmessung, Split/Trim/Fades, Dynamic Split, Razor, Swipe-Comping, Clip-Gain,
Marker/Regionen + Songgrenzen, Tempo-Spur, Konsole/Inserts/Sends/Busse, PDC, Automations-Lanes,
VST3-Hosting, Basis-FX (EQ, Komp, Gate, TP-Limiter), LUFS-Meter, Dither, Render-Dialog/Queue/pro Region,
Stems, Wildcards, LUFS-Normalisierung, Audio-Import, Action-Liste/Makros, Shortcut-Presets, Themes,
CPU-Anzeige, agentischer Claude-Assistent. Mit `†` markierte Herkünfte vor Umsetzung belegen (M0-08).

**Songgrenzen (E31, E32):** Eigenbau ohne Fremdlibrary, Offline-Job im Worker-Thread: Mono/22,05 kHz → Merkmale je
100 ms (RMS, spektrale Flachheit, ZCR, Chroma) → Klassen Stille/Applaus/Musik/Ansage → Kandidaten (Pausen, Applaus,
Novelty) → Auswahl, mit Setlist per dynamischer Programmierung für genau K Songs. Defaults: min 60 s, max 12 min,
Vorlauf 1 s, Nachklang 3 s. Bewertung: Boundary-F ±3 s ≥ 0,9; Fixture `mitschnitt-mini` generiert.

## 7. Mastering & Export (Auftrag §10)
- Formate MVP: WAV/BWF, FLAC, MP3 (Encoder-Lizenz per ADR); v1: AIFF, Ogg, DAWproject, MIDI.
- Normstand: ITU-R BS.1770-5 (2023), EBU R128 v5.0, Tech 3341 v4.0; Gates −70 LUFS / −10 LU, LRA −20 LU,
  10.–95. Perzentil; True Peak 4× (< 96 kHz) bzw. 2× Oversampling. Messung über Tracktion `LoudnessMeter`, validiert (E27).
- Lautheits-Presets: Video-Pipeline −14 LUFS/−1 dBTP (WAV 48/24), Spotify, SoundCloud (offiziell belegt), EBU R128
  (belegt), Apple, YouTube, Amazon, Tidal, Deezer (**Community-Werte**, in der UI so gekennzeichnet), Podcast (AES TD1008),
  „nur messen“ (Standard).
- Render-Pipeline: Master-Kette → Normalisierung (messen → Gain → TP-Limiter) → SRC → Dither (nur bei
  Bittiefen-Reduktion, letzte Stufe) → Encoder → Metadaten → Prüfmessung, Bericht `.json` neben der Datei.

## 8. Qualität, CI, Release (Auftrag §11)
- Tests zuerst (test-writer): Unit, Engine-Integration, Registry-Vertrag, Assistant-Simulation ohne Netz,
  MCP-Conformance, GUI-Screenshots.
- Golden-Render: `tests/fixtures/mitschnitt-mini`, `band-mini`; Null-Test < −90 dBFS; Updates nur per Karte.
- LUFS-Validierung: zuerst eigene Testsignale nach Tech-3341-Beschreibung (E28); offizielles EBU-Set erst nach Klärung
  der Nutzungsbedingungen (CI-Download mit SHA-256, nie im Repo). Test-Orakel: libebur128 (MIT).
- `tracklab-cli`: `render`, `analyze`, `find-songs`, `run-commands`, `screenshot`, `validate-lufs`, `export-tools`.
- CI-Jobs (Runner gepinnt: `ubuntu-24.04`, `windows-2025`): static → build-test-linux (GCC 14, Clang 18),
  rtsan-linux (Clang ≥ 20 – RTSan nur Linux, E30), build-test-windows (MSVC, sccache), package-linux (AppImage, .deb),
  package-windows (Inno Setup 6.7.x + ZIP, E6), smoke, docs-check. Test-Artefakte 14 Tage bei jedem Push auf `main` mit Code-Änderung
  (seit E44/E46: reine `team/`-/`docs/`-/`*.md`-Pushes ohne CI, Arbeitsbranches ohne CI).
- Kein Code-Signing vorerst (E29).
- Release (E7): Freigabe durch David → Tag `vJJJJ.MM.N` (in Cloud-Sessions setzt David den Tag); Release-Workflow prüft
  den Tag strikt per Regex `^v[0-9]{4}\.[0-9]{2}\.[0-9]+$`.

### Gate (DoD je Karte)
CMake Release + Debug ohne Warnungen in `src/` (`-Werror`) · ctest grün · Golden + LUFS grün ·
RTSan ohne Befund · clang-format/clang-tidy sauber · Secret-Scan sauber · nur Brief-Dateien geändert ·
Doku nachgezogen (DESIGN, README, `Projektinhalt.md`, `docs/commands.md`) · reviewer `APPROVE` ·
GUI-Screenshots. **Stand (M0-07):** `scripts/gate.sh` (Stufen `static|build|tidy|rtsan|all`) und `scripts/gate.ps1`
(`static|build|all`) prüfen Pflichtdateien, CLAUDE.md-Länge, Secrets, Audio-Dateien, `core.hooksPath` und
clang-format und bauen über `CMakePresets.json` Debug + Release (GCC/Clang bzw. MSVC, `-Werror`/`/WX` für eigene
Quellen) mit ctest. clang-tidy (`.clang-tidy`) und RTSan (`linux-clang-rtsan`, `scripts/rtsan.supp`, Negativtest) laufen
unter Linux; `gate.yml` ruft dieselben Skripte auf (Jobs static, build-test-linux, rtsan-linux, build-test-windows,
mp3-compare). Bis M1 ein eigenes `src/` anlegt, ist `spike/engine/` das gebaute Projekt. Golden/LUFS folgen mit den
ersten Renderern.

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

## Korrekturen am Auftrag (Rev 2)
Diese Revision geht dem Auftrag vor (Stand der Recherche 08.10.2026):
1. §5: JUCE 9 ist erschienen (9.0.3); `juce_clap_hosting` existiert nicht nachweisbar; JUCE 9 kann kein CLAP.
2. §5: Tracktion Engine `develop` = 3.5.0 (ungetaggt) bringt LUFS-Messung, Render-Queue, beliebige Kanalzahlen.
3. §7.3: „alle Tools `strict`“ ist durch API-Limits nicht möglich (siehe §4).
4. §7.5: Haiku 5.5 kann Tool Search; `tool_choice` `any`/`tool` auf Opus/Sonnet 5.5 nicht erlaubt.
5. §7.7: MCP-Spec 2026-07-28 ist aktuell.
6. §8: Screenreader nur unter Windows; Linux-Screenshots ohne Xvfb.
7. §10.2: Apple/YouTube/Amazon/Tidal/Deezer-Werte sind nicht offiziell belegt; SoundCloud ergänzt.
8. §9 Herkunftsspalte (Features bleiben, `team/research/daw-features/NOTIZEN.md`): Z. 408 → „Cubase Slice-Quantize/Audio
   Alignment, Pro Tools Beat Detective“; Z. 467 ohne Studio Pro; Z. 500 ohne Reaper; Z. 501 unbelegt; Z. 513
   Plattform-Presets = Tracklab-eigen; Z. 427 „Bitwig (Marketing)“; Z. 394 Loopback-Messung halbautomatisch.
