# Plugin-Hosting: VST3, CLAP, LV2, Sandbox – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
Gelesen (geklont, nicht gebaut): JUCE 9.0.3, tracktion_engine `develop` @ `bb38617`, free-audio/clap @ `a47f6ba`,
clap-helpers @ `29389e4`, clap-juce-extensions @ `7adee3a`, clap-host @ `c8ce3ee`, Carla @ `97a9e07`.
juce.com, ardour.org, bitwig.com gesperrt (nur Snippets).

## 1. VST3
- JUCE 9.0.3 bündelt VST3-SDK **3.8.0 unter MIT** (`juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`).
  Marke „VST“ nur nach Steinberg-Regeln.

## 2. CLAP
- **JUCE 9.0.3: weder CLAP-Hosting noch -Authoring** im Code (Roadmap-Ankündigung „Authoring in JUCE 9“ nicht eingelöst).
- `clap-juce-extensions` (MIT): nur Authoring, ausdrücklich kein Hosting.
- **`juce_clap_hosting` nicht gefunden** → Annahme im Auftrag §5 unbelegt, streichen/markieren.
- Bausteine (MIT): `free-audio/clap` 1.2.10 (Header-only, Thread-Annotationen), `clap-helpers` (`Host<>`, `PluginProxy`,
  `EventList`, `ParamQueue` …), `clap-host` (Qt-Referenz). Carla `CarlaPluginCLAP.cpp` (GPL-2.0-or-later) nur nachlesen.
- Einbindung: eigenes `ClapPluginFormat : juce::AudioPluginFormat` über `pluginFormatManager.addFormat` (wie Tracktions
  Cmajor-Format, `tracktion_PluginManager.cpp:75-78`); format-spezifische Pfade in `tracktion_ExternalPlugin.cpp`
  (`:558`, `:916`, `:933`, `:2108`) prüfen. Aufwand: mehrere Karten.

## 3. LV2 (JUCE 9.0.3, Linux)
- `JUCE_PLUGINHOST_LV2` (Default 0). **Korrektur:** JUCE bündelt lilv 0.24.12, serd, sord, sratom (ISC) – die Aussage
  „ohne lilv“ in `lizenz-und-name/NOTIZEN.md` ist falsch (Lizenzbewertung bleibt: kompatibel).
- Kann: Audio-/Control-Ports, Atom-MIDI rein/raus, Worker, State, Latenz-Port, Options, Presets als Programme.
- Grenzen: UI **nur `X11UI`** (kein GTK/Qt/extern), `getTailLengthSeconds()` TODO, Wayland nur via XWayland (ungetestet).

## 4. Out-of-Process / Sandbox
- JUCE: kein Out-of-Process-Instanz-Host; `ChildProcessCoordinator` nur als Steuerkanal.
- Bitwig (Snippet): Modi Within / Together (Default) / By manufacturer / By plug-in / Individually; „Reload Plug-in“.
- Ardour lehnt Sandboxing ab (Latenz/Kontextwechsel) – kein Vorbild dafür.
- Vorbilder mit Code: **Carla** (Prozess je Plugin, Audio über Shared-Memory-Pool, Semaphor-Handshake je Block mit Timeout,
  Non-RT über eigene Ringpuffer); **yabridge** (Sockets für Steuerung, Shared Memory für Audio, `mlock`-Limits wichtig).
- Optionen: (1) nur Scanner isoliert – erfüllt §6.5 nicht; (2) Prozess je Instanz; (3) Gruppen.
- Latenzkosten nicht gemessen: synchroner Handshake = 0 Blöcke Extra-Latenz, aber 2 Kontextwechsel/Block → **Spike nötig**.
- GUI-Einbettung (X11 Reparent/XEmbed, Windows `SetParent`) ist großer Aufwandsposten.

## 5. Scanner, Blacklist, Cache
- JUCE: `PluginDirectoryScanner` mit Dead-Man's-Pedal, `KnownPluginList` (Blacklist, XML-Cache, `CustomScanner`).
- Tracktion: Out-of-Process-Scanner (`PluginScanMasterProcess`/`ChildProcess`), opt-in über
  `EngineBehaviour::canScanPluginsOutOfProcess()` (Default false) + `startChildProcessPluginScan`; **nur VST/LADSPA/AU**
  ausgelagert (`shouldUseSeparateProcessToScan`) → LV2/CLAP würden in-process gescannt. Keine Instanz-Blacklist.

## Empfehlung für ein späteres ADR
1. Scanner für alle Formate im eigenen Prozess (Tracktion-Mechanismus erweitern), Timeout je Datei, Blacklist + Cache
   (Änderungszeit/Version), Blacklist in der GUI entfernbar.
2. CLAP: eigener Adapter (clap + clap-helpers), Mindestumfang Scan, Audio/Noten-Ports, Parameter, State, GUI, Latenz.
3. LV2: JUCE-Host nutzen, vorher Spike mit 3–5 echten Plugins.
4. Sandbox: eigenes Binary `tracklab-plugin-host`; Steuerung über Socket/Named Pipe, Audio/MIDI über Shared Memory +
   Lock-free-Ringpuffer, Semaphor/Futex mit Timeout. Audio-Thread-Seite nur Atomics, Post/Wait-mit-Timeout, vorab gemappter
   und `mlock`-ter Speicher; bei Timeout sofort Bypass + atomares Flag, Wiederherstellung im Message-Thread. Modi wie Bitwig,
   Standard „Together“; `plugin.set_sandbox_mode`. RTSan-sauber auf DAW-Seite.
5. Nicht: Signal-Handler/`siglongjmp` im Audio-Thread.

## Folgen für den Lead
- Auftrag §5/DESIGN: `juce_clap_hosting` als „nicht gefunden“ markieren; `lizenz-und-name/NOTIZEN.md` korrigiert (lilv).

## Offen für den PO (→ `TODO-PO.md`)
- **F34** Sandbox-Zeitpunkt: Empfehlung M6 nur Scanner-Isolation + Absturzerkennung, volle Sandbox nach eigenem Latenz-Spike (vor M6 einplanen).
- **F35** CLAP als eigenes Mehrkarten-Paket in v1 akzeptieren; LV2 nur mit X11-Oberflächen (sonst generische Parameteransicht).
