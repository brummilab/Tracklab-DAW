# Drittanbieter-Hinweise (Third-Party Notices)

Tracklab steht unter der **GNU Affero General Public License v3.0 (AGPL-3.0-only)**, siehe `LICENSE`. Diese Datei
nennt die Bibliotheken, die Tracklab einbindet, mit Version und Lizenz. Die vollständigen Lizenztexte liegen in den
Quellordnern unter `third_party/` (Git-Submodule) und in den jeweils genannten Pfaden.

Pflege: Bei jeder neuen oder aktualisierten Bibliothek diese Datei im selben Merge anpassen (Karte M1-06 legt sie an).
Mit `[VERIFIZIEREN]` markierte Angaben sind noch nicht an der Quelle geprüft (R12).

## Eingebundene Bibliotheken

| Bibliothek | Version / Stand | Lizenz | Verwendung | Quelle / Lizenztext |
|---|---|---|---|---|
| JUCE | 9.0.3 (`be29c81`) | AGPL-3.0 (JUCE-Lizenzoption) | GUI, Audio-Geräte, Plugin-Hosting, Dateiformate | `third_party/JUCE/LICENSE.md` |
| Tracktion Engine | `develop` @ `bb38617` (3.5.0) | GPL-3.0-or-later | Audio-Engine, Edit, Transport, Aufnahme | `third_party/tracktion_engine/LICENSE.md` |
| VST 3 SDK (in JUCE enthalten) | 3.8 | MIT | VST3-Hosting | `third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` |
| ASIO SDK (in JUCE enthalten) | Steinberg 2025 | **GPL-3.0** (Option der Steinberg-Doppellizenz), siehe unten | ASIO-Treiber unter Windows (`JUCE_ASIO=1`) | `third_party/JUCE/modules/juce_audio_devices/native/asio/LICENSE.txt` |
| nlohmann/json | v3.12.0 | MIT | JSON in Command-Registry, Schemas, Projektdateien | `third_party/nlohmann_json/LICENSE.MIT` |
| pboettch/json-schema-validator | 2.4.0 | MIT | Validierung der Command-Parameter (JSON Schema draft 7) | `third_party/json-schema-validator/LICENSE` |
| doctest | 2.4.11 (aus dem Tracktion-Pin) | MIT | nur Tests (`tests/`), nicht im ausgelieferten Programm | `third_party/tracktion_engine/modules/3rd_party/doctest/` |

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
mitkompiliert; ihre Lizenzen liegen in den jeweiligen Ordnern. Eine Prüfung der einzelnen Lizenztexte ist noch offen
`[VERIFIZIEREN]`, vor dem ersten Release abzuschließen.

| Komponente | Ordner |
|---|---|
| choc, crill, expected, libsamplerate, magic_enum, rigtorp, rpmalloc | `third_party/tracktion_engine/modules/3rd_party/` |
| airwindows, signalsmith-stretch, soundtouch | `third_party/tracktion_engine/modules/tracktion_engine/3rd_party/` |
| FLAC, Ogg, Vorbis, Opus (Codecs) | `third_party/JUCE/modules/juce_audio_formats/codecs/` |

## Noch nicht eingebunden (geplant)

MP3-Kodierer (LAME o. ä., eigener ADR), CLAP-Adapter (clap, clap-helpers), libcurl: bei der Aufnahme in den Build hier
ergänzen.
