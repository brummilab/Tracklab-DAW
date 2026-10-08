# Engine-Spike M0-06 – Bericht

Stand: 08.10.2026 · Karte M0-06 · Lauf: `implementer-rt` · Branch `m0-06-impl` · Grundlage für ADR-001.
Windows-Werte kann nur die CI liefern: überall als **„folgt aus CI“** markiert.

## 1. Pins und Ergebnis „Tracktion develop + JUCE 9.0.3“
| Abhängigkeit | Pin | Commit |
|---|---|---|
| JUCE | Tag `9.0.3` | `be29c81492b6151c8ea8d14c840e1311963b3a83` |
| Tracktion Engine | `develop` (3.5.0, ungetaggt) | `bb386171ca15d36d8d0827cf8feb2d32e39f11fe` |

- **Tracktion `develop` baut mit JUCE 9.0.3** – unter GCC 13.3 (Release) und Clang 18.1 (Debug) ohne Fehler und ohne
  eine einzige Warnung im Modulcode. Kein Rückfall auf JUCE 8.0.15 / Tracktion v3.2.0 nötig. Windows/MSVC: folgt aus CI.
- Einbindung wie geplant: Submodule, `add_subdirectory` JUCE vor Tracktion, Tracktions `modules/juce` nicht geklont,
  `TE_ADD_EXAMPLES=OFF`. Modulcode wird genau einmal kompiliert (`spike_modules`), dazu einmal JUCE für das Plugin.

## 2. Testergebnis (lokal, Ubuntu 24.04, 4 Kerne, 15 GB RAM)
57 doctest-Fälle in 6 CTest-Suiten, `SPIKE_REQUIRE_TOOLS=1` (lame 3.100, ffmpeg 6.1.1), alle grün, 0 übersprungen.

| Suite | GCC 13.3 Release | Clang 18.1 Debug |
|---|---|---|
| import | grün, 0,18 s | grün, 0,23 s |
| render | grün, 8,5 s | grün, 17,1 s |
| record12 | grün, 2,7 s | grün, 4,2 s |
| vst3 | grün, 0,24 s | grün, 0,29 s |
| mp3 | grün, 1,3 s | grün, 1,7 s |
| cli | grün, 1,2 s | grün, 1,9 s |
| **gesamt** | **14,2 s** | **25,5 s** |

Debug-Lauf: zwei JUCE-`jassert` auf stderr, beide erwartet: `juce_Midi_linux.cpp:539` (kein ALSA-Sequencer
`/dev/snd/seq` im Container; MIDI-Scan des DeviceManagers) und `juce_VST3PluginFormatImpl.h:1117` (Negativtest
„kein VST3-Bundle“). Windows: folgt aus CI.

## 3. Ergebnisse je Minimal-Ziel
**import / dump-pcm** – über Tracktions Importpfad (`te::AudioFile`, `AudioFileUtils::createReaderFindingFormat`).
WAV 16/24/32-float, 44,1/48/96 kHz: Länge, Rate, Kanäle exakt.
- MP3 (lame 128 kbit/s): JUCEs `MP3AudioFormat` wertet die LAME-Gapless-Info **nicht** aus. 2 s bei 44,1 kHz
  (88 200 Samples) → 89 856 Samples (78 Frames); 48 kHz: 96 000 → 97 920. Gegenüber ffmpeg um **1105 Samples**
  (Encoder-Delay) verschoben; nach Ausrichtung Abweichung −125 dB. Dekodierung deterministisch (bit-identisch).
  **Folge für Tracklab:** importierte MP3 liegen ~25 ms zu spät und sind länger → eigene Karte (Gapless-Info lesen
  oder ffmpeg-Dekoder) `[VERIFIZIEREN]`.
- Windows vs. Linux sample-gleich: folgt aus CI (Job `mp3-compare`).

**render-region** – `RenderSpecification` → `createRenderJob` → `Renderer::renderToFile`, 48 kHz/24 Bit.
- Länge sample-genau (auch 44,1-kHz- und MP3-Quellen), Regionsinhalt sample-genau (Pegelsprung landet exakt).
- Lautheit mit Tracktions `LoudnessMeter` über die gerenderte Datei: −20/−30 dBFS-Sinus ±0,1 LU, True Peak ±0,1 dB,
  LRA 10 LU (±0,3) und 0 LU (±0,1). Werte innerhalb der Toleranzen (Tests grün).
- **Render-API-Wahl `RenderSpecification`:** reine Daten mit kanonischer JSON-Form (`toJSON`/`fromJSON`) – passt zur
  Tracklab-Regel „jede Aktion ist ein Command mit JSON-Schema“; `Renderer::Parameters` bleibt Implementierungsdetail.
  Nötig: `includeTails = false` (sonst Nachlauf bis 10 s), `setUsesProxy(false)` am Clip, eigener
  `UIBehaviour::runTaskWithProgressBar` (Default ist `jassertfalse` und tut nichts).

**record-12** – `HostedAudioDeviceInterface` (12 Ein-, 2 Ausgänge, 48 kHz, 512er Blöcke), 12 Mono-Eingänge auf
12 Spuren, eigener Treiber statt `EnginePlayer` (der allokiert pro Block). 5 s: 12 Dateien à 240 000 Samples,
`missing_blocks = 0`, Abweichung **−138,6 dB** (24-Bit-Quantisierung). 1 s (93,75 Blöcke, kurzer letzter Block): exakt.
- **Latenz/Blockversatz:** Hosted Device mit 0 Ein-/Ausgangslatenz: Graph-Latenz 0, Clip-Start 0, kein Versatz.
  Tracktion verwirft am Aufnahmeanfang `getRecordAdjustmentSamples()` = Ein- **plus** Ausgangslatenz des Geräts
  (Overdub-Modell: Aufnahme wird um die Ausgangslatenz *früher* gelegt als am physischen Eingang angekommen) und
  `getLatencySamples()` des Graphen; die gleiche Zahl Samples muss am Ende nachgeschoben werden. Laut Tracktions eigenen
  Tests (`tracktion_WaveInputDevice.test.cpp`, „loop-back“) bleibt bei Loopback **ein Block Versatz**, den
  `LatencyTester` korrigiert. Im Spike nicht gemessen (keine Gerätelatenz simuliert) → bei `io.measure_latency`
  `[VERIFIZIEREN]`.
- `setCpuLimitBeforeMuting(1000)` nötig, weil schneller als Echtzeit gepumpt wird (sonst stummgeschaltete Blöcke) –
  wie Tracktions `EnginePlayer`.

**load-vst3** – eigenes `SpikeGain.vst3` (Parameter `gainDb`, −24…+12 dB, Default −6 dB, ohne Smoothing, kein
Editor), gescannt und geladen über `engine.getPluginManager().pluginFormatManager`, ein Block 4800 Frames:
Default −6,00 dB, +3, −12, 0 dB jeweils ±0,01 dB. **xvfb nicht nötig:** alle Tests liefen ohne `DISPLAY`.

## 4. Build-Zeiten und Cache (lokal, 4 Kerne, Ninja, ccache 4.9.1, eigenes CCACHE_DIR)
| Compiler/Config | Configure | Build kalt | Build warm | ccache | Build-Ordner |
|---|---|---|---|---|---|
| GCC 13.3 Release | 35 s | 275 s | 57 s | 33 MB | 273 MB |
| Clang 18.1 Debug | 28 s | 152 s | 157 s ¹ | 135 MB | 1,4 GB |
| MSVC Debug/Release | folgt aus CI | folgt aus CI | folgt aus CI | sccache: CI | CI |

¹ „Warm“ = neuer Build-Ordner mit vollem Cache. Bei Debug (`-g`) hasht ccache das Arbeitsverzeichnis mit → fast
nur Fehltreffer. In CI ist der Pfad gleich (`build/`), dort sind Treffer zu erwarten; lokal hilft `CCACHE_NOHASHDIR=1`
bzw. `base_dir`. Kalt-Messungen liefen teils parallel zu anderen Prozessen (Richtwerte). Runner/Cache-Größen der CI:
folgt aus CI (Step-Summary des Workflows).

## 5. apt-Pakete (Linux)
Belegt über die tatsächlich eingebundenen Systemheader (`ninja -t deps`) und gelinkten Bibliotheken:
```
sudo apt install build-essential cmake ninja-build pkg-config ccache \
  libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxext-dev libxrender-dev libxrandr-dev libxinerama-dev libxi-dev libxcursor-dev \
  lame ffmpeg            # nur für die Tests
```
Gelinkt werden nur `libasound`, `libfreetype`, `libfontconfig` (+ libc/pthread/dl/rt/atomic).
**Nicht nötig** (WebKit ist im Container gar nicht installiert, Build läuft): `libwebkit2gtk-4.1-dev` – im
CI-Workflow entfernt. Vermutlich ebenfalls nicht nötig, aber nicht durch Deinstallation geprüft:
`libjack-jackd2-dev`, `ladspa-sdk`, `libcurl4-openssl-dev`, `libglu1-mesa-dev`, `mesa-common-dev`, `libegl-dev`,
`libxcomposite-dev`, `xvfb` `[VERIFIZIEREN]` (bleiben bis dahin im Workflow).

## 6. Echtzeit-Beobachtung: Tracktions Aufnahmepfad im Callback
Methode: gdb-Skript zählt `malloc/calloc/realloc/free(≠NULL)/pthread_mutex_lock` auf dem Thread, der
`HostedAudioDeviceInterface::processBlock` ausführt, während es läuft (`record-12 --seconds 1`, GCC Release,
94 Blöcke). Worker-Threads des Graphen sind nicht erfasst; `shared_mutex`/SpinLock nicht gezählt.
- **67 Allokationen** in 94 Blöcken: Block 1: 42 (Aufwärmen: `AudioBuffer::setSize`/`choc`-Puffer in
  `WaveInputDeviceInstance::acceptInputBuffer`, `LevelMeasurer::Client`), Block 2–37 vereinzelt (7), letzter
  (kürzerer) Block: 15 (`setSize` bei geänderter Blockgröße, `WaveInputRecordingThread::addBlockToRecord`).
  Dazu kommt `new QueuedBlock()`, sobald der Pool von 32 Blöcken leer ist (bei 12 Eingängen schnell möglich).
- **~78 Mutex-Locks pro Block** (7330 gesamt): `CriticalSection` in `acceptInputBuffer` (consumerLock) und in der
  Block-Queue des Aufnahme-Threads (2 je Eingang und Block), `Thread::notify()` je Eingang, `instanceLock`,
  `LiveMidiInjectingNode`, JUCEs `AudioDeviceManager`-Callback-Lock, Hosted-MIDI-Lock, `Edit::updateModifierTimers`.
- Der Spike-eigene Pfad (Treiber `HostedDeviceDriver::feed`, `SpikeGain::processBlock`) allokiert und lockt nicht.
- **Bewertung:** Tracktion erfüllt `docs/realtime.md` im Aufnahmepfad nicht wörtlich (Locks sind meist unkontendiert,
  Allokationen vor allem beim Start/Größenwechsel). Für M0-07 (RTSan im Gate) heißt das: RTSan wird in Tracktion-Code
  Befunde melden → Suppressions-Liste oder Ausnahmen für Fremdcode nötig; kritische Stellen ggf. upstream melden.
- `[[clang::nonblocking]]` gibt es erst ab Clang 20; lokal und auf `ubuntu-24.04` ist Clang 18 → Makro
  `SPIKE_NONBLOCKING` ist dort leer. Ab Clang 20 würden Aufrufe in nicht annotierten JUCE/Tracktion-Code
  `-Wfunction-effects` auslösen (mit `-Werror` Build-Fehler) → in M0-07 entscheiden.

## 7. Blocker, Workarounds, Abweichungen
1. **Test korrigiert (nachweislich falsch):** `render` „True Peak … fs/6 … (−6,02 dBTP)“. Die Datei beginnt bei
   60° Phase mit einem Sprung 0 → 0,433 und endet mit maximaler Steigung. Ihre ideale bandbegrenzte Rekonstruktion
   (Sinc, numpy) hat am Rand **0,526 = −5,58 dBTP** (Mitte −6,02). Tracktions Meter misst −5,59 dB (korrekt);
   ffmpeg zeigt −6,0, weil sein Interpolator den Randüberschwinger unterschätzt. Korrektur: 10 ms Ein-/Ausblendung
   im Fixture, Erwartungswert unverändert (−6,02 ±0,1). Danach grün.
2. **CMake-Fehler im Gerüst:** `--spike-gain-vst3=$<TARGET_PROPERTY:…,JUCE_PLUGIN_ARTEFACT_FILE>` lieferte einen
   unaufgelösten Generator-Ausdruck → mit `$<TARGET_GENEX_EVAL:…>` behoben.
3. **Windows-Linkfehler erwartet:** Mit `JUCE_USE_WINDOWS_MEDIA_FORMAT=0` deklariert JUCE die Klasse
   `WindowsMediaAudioFormat` weiter, definiert sie aber nicht; Tracktions `AudioFileFormatManager` instanziiert sie
   unter `#elif JUCE_WINDOWS` ohne Flag-Prüfung. Workaround `src/windows_media_disabled.cpp` (Platzhalter ohne
   Dateiendungen). Ungeprüft → folgt aus CI `[VERIFIZIEREN]`; sauber wäre ein Upstream-Fix in Tracktion.
4. Tracktion schreibt sonst `Settings.xml` in `~/.config/<App>`: im Spike `PropertyStorage` im Speicher, Prefs/Cache
   in einem Temp-Ordner je Engine. `autoInitialiseDeviceManager()`/`addSystemAudioIODeviceTypes()` = false (headless).
5. stderr-Rauschen: „ALSA lib seq_hw.c … /dev/snd/seq failed“ (MIDI-Scan ohne Sequencer) – harmlos.
6. `Record12Result` um zwei Beobachtungsfelder ergänzt (`graphLatencySamples`, `clipStartSamples`; JSON
   `graph_latency_samples`, `clip_start_samples`) – von Tests nicht geprüft.

## 8. Offene Risiken (Windows/MSVC)
- Build/Tests unter MSVC 2022 `/W4 /WX` komplett ungeprüft; Workaround aus 7.3 nicht kompiliert.
- JUCEs `jassert`-Verhalten und VST3-`moduleinfo.json`-Erzeugung (Helper läuft beim Build) unter Windows.
- MP3-Plattformvergleich und Windows-Build-Zeiten/sccache-Trefferquote: nur CI.

## 9. Empfehlung für ADR-001
**JUCE 9.0.3 + Tracktion Engine `develop` @ `bb38617` übernehmen** (Linux belegt, Windows nach grüner CI):
alle vier Minimal-Ziele mit Bordmitteln erfüllt, Messgenauigkeit (LUFS/TP/LRA, sample-genaue Regionen, −138 dB
Aufnahmetreue) ausreichend, Build moderat (≈4,5 min kalt, <1 min warm). Auflagen für die Folgekarten:
1. Echtzeit: Tracktions Aufnahmepfad allokiert/lockt (Abschnitt 6) → RTSan-Strategie für Fremdcode in M0-07.
2. MP3-Import: Gapless-Info fehlt (1105 Samples Versatz) → eigene Karte.
3. Upstream-Abhängigkeit `develop` (ungetaggt) bewusst pinnen; WindowsMedia-Workaround upstream melden.
