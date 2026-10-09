# Drittanbieter-Hinweise (Third-Party Notices)

Tracklab steht unter der **GNU Affero General Public License v3.0 (AGPL-3.0-only)**, siehe `LICENSE`. Diese Datei
nennt die Bibliotheken, die Tracklab einbindet, mit Version und Lizenz. Die vollständigen Lizenztexte liegen in den
Quellordnern unter `third_party/` (Git-Submodule) und in den jeweils genannten Pfaden.

Pflege: Bei jeder neuen oder aktualisierten Bibliothek diese Datei im selben Merge anpassen (Karte M1-06 legt sie an).
Mit `[VERIFIZIEREN]` markierte Angaben sind noch nicht an der Quelle geprüft (R12). Stand der Prüfung der
mitgelieferten Komponenten: 08.10.2026 (Karte M1-06).

## Eingebundene Bibliotheken

| Bibliothek | Version / Stand | Lizenz | Verwendung | Quelle / Lizenztext |
|---|---|---|---|---|
| JUCE **mit Tracklab-Patch** | 9.0.3 (`be29c81`) | AGPL-3.0 (JUCE-Lizenzoption) | GUI, Audio-Geräte, Plugin-Hosting, Dateiformate | `third_party/JUCE/LICENSE.md`, Patch siehe unten |
| Tracktion Engine | `develop` @ `bb38617` (3.5.0) | GPL-3.0-or-later | Audio-Engine, Edit, Transport, Aufnahme | `third_party/tracktion_engine/LICENSE.md` |
| VST 3 SDK (in JUCE enthalten) | 3.8 | MIT | VST3-Hosting | `third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` |
| ASIO SDK (in JUCE enthalten) | 2.3 (© 2025 Steinberg) | **GPL-3.0** (Option der Steinberg-Doppellizenz), siehe unten | ASIO-Treiber unter Windows (`JUCE_ASIO=1`) | `third_party/JUCE/modules/juce_audio_devices/native/asio/LICENSE.txt` |
| nlohmann/json | v3.12.0 | MIT | JSON in Command-Registry, Schemas, Projektdateien | `third_party/nlohmann_json/LICENSE.MIT` |
| pboettch/json-schema-validator | 2.4.0 | MIT | Validierung der Command-Parameter (JSON Schema draft 7) | `third_party/json-schema-validator/LICENSE` |
| doctest | 2.4.11 (aus dem Tracktion-Pin) | MIT | nur Tests (`tests/`), nicht im ausgelieferten Programm | `third_party/tracktion_engine/modules/3rd_party/doctest/` |

## Änderung an JUCE (Tracklab-Patch)

Tracklab verändert den Quelltext von JUCE an einer Stelle: `third_party/patches/juce-undomanager-stale-stash.patch`
ändert `modules/juce_data_structures/undomanager/juce_UndoManager.cpp` (wenige Zeilen). Der Patch leert den internen
Zwischenspeicher („Stash“) früherer Redo-Schritte beim Beginn einer neuen Transaktion und in `clearUndoHistory()`.
Grund: Ohne ihn kommt ein längst verworfener Redo-Schritt nach `undoCurrentTransactionOnly()` als „Geister-Redo“ zurück
(Entscheidung E49, Recherche `team/research/juce-undo-stash/NOTIZEN.md`). `cmake/TracklabDeps.cmake` wendet ihn beim
Konfigurieren an (idempotent); das Submodul-Pin bleibt unverändert. Die Änderung steht wie JUCE selbst unter der AGPL-3.0,
der Quelltext des Patches liegt im Repository. Entfällt, sobald JUCE den Fehler upstream behebt.

## Audio-Schnittstellen des Betriebssystems

Das sind Systemschnittstellen, keine mitgelieferten Bibliotheken.

| Schnittstelle | System | Hinweis |
|---|---|---|
| WASAPI | Windows | Wird über JUCE angesprochen: **geteilt** (shared), **exklusiv** (exclusive) und **niedrige Latenz** (shared low latency, `IAudioClient3`). Empfehlung: ASIO bevorzugen, WASAPI exklusiv als Rückfall. |
| ASIO | Windows | Treiber des Geräteherstellers; „ASIO“ ist eine Marke der Steinberg Media Technologies GmbH, die Markenrichtlinien sind zu beachten (siehe unten). |
| ALSA | Linux | Über JUCE (`JUCE_ALSA=1`), Systembibliothek `libasound`. |
| JACK-API | Linux | Über JUCE (`JUCE_JACK=1`); `libjack.so.0` wird zur Laufzeit per `dlopen` geladen, in der Praxis die von PipeWire (`pipewire-jack`, Start mit `pw-jack`). Nicht mitgeliefert. |

## ASIO-SDK: Lizenzhinweis

Das in JUCE enthaltene ASIO-SDK von Steinberg ist doppelt lizenziert: nach der proprietären Steinberg-ASIO-Lizenz
**oder** nach der **GNU General Public License Version 3**. Tracklab nutzt das SDK **ausschließlich unter der
GPLv3-Option**. Das ist mit der AGPL-3.0 von Tracklab vereinbar. Folgen:

- Builds mit ASIO werden nur zusammen mit dem vollständigen Quelltext (AGPLv3/GPLv3) weitergegeben.
- Die proprietäre Steinberg-ASIO-Lizenzvereinbarung wird nicht abgeschlossen und nicht benötigt.
- Der Build-Schalter ist `JUCE_ASIO=1` (`cmake/TracklabDeps.cmake`); das SDK wird aus JUCE verwendet
  (`JUCE_ASIO_USE_EXTERNAL_SDK=0`), nicht aus einer getrennt heruntergeladenen Kopie.
- Marke: „ASIO“ ist eine eingetragene Marke der Steinberg Media Technologies GmbH. Die Markenrichtlinien von Steinberg
  sind einzuhalten; die genaue Formulierung und das ASIO-Logo `[VERIFIZIEREN]`
  (Quelle: Steinberg-Entwicklerseite, siehe `team/research/audio-backends-linux/NOTIZEN.md` §3).

## Von JUCE und Tracktion mitgelieferte Komponenten

Beide Projekte bringen eigene Drittanbieter-Quellen mit. Sie werden über die Module von JUCE und Tracktion Engine
mitkompiliert. Geprüft am 08.10.2026 anhand der Lizenz-/Kopfzeilen in `third_party/` und der Stückliste
`third_party/JUCE/JUCE.spdx.json` (SPDX-Kennungen); alle Lizenzen sind mit der AGPL-3.0 von Tracklab vereinbar.

### Aus Tracktion Engine (`third_party/tracktion_engine/modules/`)

| Komponente | Lizenz | Nachweis | Im Build |
|---|---|---|---|
| choc | ISC | `3rd_party/choc/LICENSE.md` | ja |
| crill | Boost Software License 1.0 | `3rd_party/crill/LICENSE.txt` | ja |
| expected (tl::expected) | CC0-1.0 | Kopfzeile von `3rd_party/expected/expected.hpp` | ja |
| libsamplerate | BSD-2-Clause | Kopfzeile von `3rd_party/libsamplerate/samplerate.h` | ja |
| magic_enum 0.9.3 | MIT | Kopfzeile von `3rd_party/magic_enum/magic_enum.hpp` | ja |
| rigtorp MPMCQueue | MIT | Kopfzeile von `3rd_party/rigtorp/MPMCQueue.h` | ja |
| rpmalloc | Public Domain | Kopfzeile von `3rd_party/rpmalloc/rpmalloc.h` | ja |
| airwindows (Effekte) | MIT (Chris Johnson) | Im Tracktion-Ordner steht je Datei nur „Copyright airwindows, All rights reserved“, **kein** Lizenztext. Die Lizenz MIT steht in `LICENSE` des Upstream-Projekts `github.com/airwindows/airwindows` (am 08.10.2026 abgerufen). Beim Release den MIT-Text mitliefern. | ja |
| signalsmith-stretch (+ signalsmith-linear) | MIT | `tracktion_engine/3rd_party/signalsmith-stretch/LICENSE.txt` | **nein** (`TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH` ist 0) |
| SoundTouch | **LGPL-2.1-or-later** | Kopfzeile von `tracktion_engine/3rd_party/soundtouch/source/SoundTouch/SoundTouch.cpp` | **nein** (`TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH` ist 0) |
| doctest 2.4.11 | MIT | siehe Tabelle oben | nur Tests |

Hinweis zur Zeitdehnung: Wird später ein Zeitdehnungs-Verfahren eingeschaltet, hier eintragen. Signalsmith (MIT) ist
unproblematisch. SoundTouch (LGPL-2.1+) ist mit der AGPL vereinbar, verlangt aber den Lizenztext und den Hinweis auf die
Bibliothek in der Auslieferung.

### Aus JUCE (`third_party/JUCE/modules/`, Stückliste `JUCE.spdx.json`)

Nur Komponenten der Module, die Tracklab kompiliert (juce_core, juce_events, juce_data_structures, juce_graphics,
juce_gui_basics, juce_gui_extra, juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors,
juce_audio_processors_headless, juce_audio_utils, juce_dsp, juce_osc).

| Komponente | Version | Lizenz (SPDX) | Modul |
|---|---|---|---|
| zlib | 1.3.2 | Zlib | `juce_core` |
| FLAC | 1.5.0 | BSD-3-Clause | `juce_audio_formats` |
| libogg, libvorbis | 1.3.6, 1.3.7 | BSD-3-Clause | `juce_audio_formats` |
| Opus, opusfile, libopusenc | 1.6.1, 0.12, 0.3 | BSD-3-Clause | `juce_audio_formats` |
| libpng | 1.6.58 | libpng-2.0 | `juce_graphics` |
| libjpeg | 10.0 | IJG | `juce_graphics` |
| libwebp | 1.6.0 | BSD-3-Clause | `juce_graphics` |
| HarfBuzz | 14.2.1 | MIT-Modern-Variant | `juce_graphics` |
| SheenBidi | 2.9.0 | Apache-2.0 | `juce_graphics` |
| LunaSVG, PlutoVG | 3.5.0, 1.3.2 | MIT | `juce_graphics` |
| VST 3 SDK | 3.8.0 | MIT | siehe Tabelle oben |
| ASIO SDK | 2.3 | Steinberg-ASIO **oder** GPL-3.0 | siehe Tabelle oben und ASIO-Abschnitt |
| LV2 (lilv, serd, sord, sratom) | 1.18.0 und folgende | ISC | `juce_audio_processors_headless/format_types/LV2_SDK`, nur mit LV2-Hosting (nicht eingeschaltet) |

Nicht verwendet, obwohl in JUCE enthalten: AAX SDK, AudioUnit SDK, Oboe (Android), Box2D, QuickJS, reaper-sdk, die
JUCE-Beispiele (ISC). Fonts: Die von JUCE gerenderten Schriften sind Systemschriften, es wird keine Schrift
mitgeliefert `[VERIFIZIEREN]` mit der GUI-Karte (M1-08), sobald eine Schrift ausgeliefert wird.

## Noch nicht eingebunden (geplant)

MP3-Kodierer (LAME o. ä., eigener ADR), CLAP-Adapter (clap, clap-helpers), libcurl: bei der Aufnahme in den Build hier
ergänzen.
