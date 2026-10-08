# Engine-Spike – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead. Methode: JUCE (Tags 9.0.3, 8.0.15) und Tracktion Engine
(`develop` @ `bb38617`) geklont und Quelltext gelesen; **nicht gebaut** (Dev-Pakete fehlen in der Sandbox).

## 1. JUCE – Korrektur: JUCE 9 ist erschienen
- Tags (github.com/juce-framework/JUCE): `9.0.0` (21.07.2026), `9.0.1`, `9.0.2`, **`9.0.3` = `be29c81` (28.09.2026, Latest)**;
  `8.0.15` = `91ad83a` (21.07.2026).
- 9.x: Linux braucht `libxi-dev` (oder `JUCE_USE_XINPUT=0`) und `libegl-dev` (EGL statt GLX); 9.0.1 baut zlib/libjpeg/
  libpng/libflac als C; Modul `juce_audio_processors_headless` (Hosting ohne UI). CMake ≥ 3.22.
- CLAP in 9.0.x-Release-Notes nicht erwähnt → Hosting weiter selbst `[VERIFIZIEREN]`.
- Lizenz: Module AGPLv3/kommerziell (`LICENSE.md`).
- **Auftrag §5 und DESIGN („JUCE 9 noch nicht erschienen“) korrigieren** → DESIGN Rev 2.

## 2. Tracktion Engine
- Letzter Tag **`v3.2.0` = `0a5f4e6` (15.05.2025)**. Default-Branch `develop` @ `bb38617` (07.10.2026), `VERSION.md` **3.5.0
  ohne Tag**.
- v3.5 (`CHANGELIST.md`): beliebige Kanalzahlen, `RenderSpecification`/`RenderQueue` (Stems, JSON, Abbruch),
  `normaliseByLUFS`/`limitTruePeak`, `LoudnessMeter` (R128 M/S/I, Tech 3342 LRA, True Peak), `AudioFileAnalyser`,
  Folder-Projekte, doctest. Deckt Workflow A und Mehrkanal-Aufnahme mit Bordmitteln ab.
- Lizenz `LICENSE.md`: „dual GPL3 (or later)/Commercial“ (Modul-Header sagt widersprüchlich `Proprietary` – `LICENSE.md` maßgeblich).
- JUCE-Pin im Tracktion-Submodul: `37c894f` (21.05.2026, 8.0.13-develop); CI `juce_compat.yaml` baut täglich gegen
  JUCE-`develop`. **Kompatibilität mit JUCE 9.0.3 unbelegt** → erste Prüfung im Spike.

## 3. Einbindung
- JUCE-Doku: `add_subdirectory(JUCE)` empfohlen (alternativ `find_package`). Tracktion: nutzt JUCE des Eltern-Projekts,
  wenn `juce::juce_core` existiert; `TE_ADD_EXAMPLES=OFF`.
- **Empfehlung:** beide als Git-Submodule mit festem Pin, JUCE zuerst einbinden; Tracktion-Submodul `modules/juce`
  **nicht** rekursiv klonen.

## 4. MP3
- Dekoder `MP3AudioFormat`: JUCE 8.0.15 Default **0** (mit Haftungsausschluss), JUCE 9.0.3 Default **1** (ohne).
  → `JUCE_USE_MP3AUDIOFORMAT=1` explizit setzen.
- Windows-Media-Format wird nach MP3 registriert; Vorschlag `JUCE_USE_WINDOWS_MEDIA_FORMAT=0` für plattformgleiche Dekodierung.
- Patentlage `[VERIFIZIEREN]`.
- Export: Tracktion hat keinen Encoder; `LAMEManager` nutzt externes `lame`, LibLame per Laufzeit-Laden
  (`TRACKTION_ENABLE_LIBLAME`) oder FFmpeg → spätere Karte/ADR.

## 5. Headless-CI
- `HostedAudioDeviceInterface` (Engine öffnet kein Gerät; Kanäle, Samplerate, Blockgröße frei) und
  `test_utilities::EnginePlayer`. Engine-Test „Multi-channel recording“ (`tracktion_WaveInputDevice.test.cpp`) ist das
  Muster für 12 Eingänge (Vergleich ≤ −99 dB). JUCE selbst hat kein Null-Device.

## 6. VST3
- `JUCE_PLUGINHOST_VST3=1`; VST3-SDK (MIT) im Modul `juce_audio_processors_headless`. Test-Plugin selbst bauen
  (`juce_add_plugin(SpikeGain FORMATS VST3)`), kein Fremd-Binary (R13).

## 7. Build/Caching
- Keine Messwerte (nicht gebaut). `hendrikmuhs/ccache-action` (v1.2.9), `Mozilla-Actions/sccache-action` (v0.0.9);
  MSVC: `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` (CMP0141). Runner-Größe hängt an Repo-Sichtbarkeit `[VERIFIZIEREN]`.
- Build-Zeit sparen: `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0` (wie Tracktions TestRunner).

## 8. Linux-Pakete (JUCE `docs/Linux Dependencies.md`, 9.0.3)
```
sudo apt install libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev \
  libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev libegl-dev
```
dazu `cmake`, `ninja-build`, Compiler; für Tracklab `libsecret-1-dev`, `xvfb` (nur GUI-Tests).

## Offen für den PO (→ `TODO-PO.md`)
- F24 JUCE 9.0.3 vs. 8.0.15 – Spike prüft zuerst 9.0.3 mit Tracktion `develop`.
- **F26** Tracktion `develop` (3.5.0, ungetaggt, fester Commit) vs. Tag `v3.2.0`.
- **F27** LUFS/Render: Engine-Funktionen nutzen und gegen EBU validieren vs. eigene Messung.
- MP3-Export-Pfad → spätere Karte. Repo-Sichtbarkeit → F1.

Der Brief-Vorschlag des Researchers ist in `team/board/backlog/M0-06.md` eingearbeitet.
