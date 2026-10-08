# ADR-001: Tech-Stack und Lizenz

- **Status:** **Angenommen** (E39, 08.10.2026). Spike M0-06 erfolgreich unter Linux (GCC, Clang) und Windows (MSVC),
  MP3-Dekodierung bitidentisch (`team/research/engine-spike/BERICHT.md` §11).
- **Datum:** 08.10.2026
- **Entscheidungen:** E39 (Annahme), E2, E3, E21, E24, E26, E30, E36 (`team/ENTSCHEIDUNGEN.md`)
- **Belege:** `team/research/engine-spike/`, `lizenz-und-name/`, `audio-backends-linux/`, `plugin-hosting-clap-lv2/`,
  `ci-packaging/`, `claude-api/` (jeweils `NOTIZEN.md`)

## Kontext
Tracklab braucht eine stabile Audio-Engine für Mehrspur-Aufnahme, Offline-Render und Plugin-Hosting unter Windows 10/11
und Linux Mint/Ubuntu, eine native GUI und eine Command-Registry, die GUI, Claude, MCP und CLI gemeinsam nutzen.
Workflow A (Live-Mitschnitt → Songs → Lautheit → Export) muss zuerst funktionieren.

## Entscheidung
| Baustein | Wahl | Pin |
|---|---|---|
| Sprache/Build | C++20, CMake ≥ 3.25, Ninja; MSVC, GCC, Clang; x64 | – |
| Framework | JUCE (AGPLv3) | `9.0.3` (`be29c81`), Rückfall `8.0.15` (`91ad83a`) |
| Engine | Tracktion Engine (GPLv3-or-later) | `develop` @ `bb38617` (3.5.0), Rückfall `v3.2.0` (`0a5f4e6`) |
| Einbindung | Git-Submodule, `add_subdirectory` (JUCE vor Tracktion, Tracktions JUCE-Submodul ungenutzt) | – |
| Audio | ASIO (SDK unter GPLv3-Option), WASAPI; JACK-API über pipewire-jack, ALSA | – |
| Plugins | VST3 (MIT-SDK in JUCE); LV2 (JUCE); CLAP eigener Adapter (clap/clap-helpers, MIT) | – |
| HTTP | libcurl direkt | – |
| JSON | nlohmann/json (MIT) | – |
| Tests | doctest (aus dem Tracktion-Pin, MIT) + CTest | 2.4.11 |
| Lizenz Tracklab | **AGPL-3.0-only** | – |

**Lizenzfolge:** AGPLv3 §13 erlaubt die Kombination der AGPL-Teile (JUCE) mit GPLv3-Teilen (Tracktion); das Gesamtwerk
wird unter AGPLv3 weitergegeben. Closed Source bräuchte kommerzielle Lizenzen von JUCE **und** Tracktion.

## Bewertete Alternativen
| Alternative | Bewertung |
|---|---|
| Ardour-Fork | Vollständige DAW (GPLv2+), aber GTK-GUI, eigene Architektur, kein Command-Registry-Ansatz; Umbau größer als Neubau auf einer Engine. Kein Sandboxing (bewusst abgelehnt). |
| Rust-Stack | Kein reifes Engine-Äquivalent zu Tracktion; Plugin-Hosting und GUI selbst bauen; Auftrag: nicht für v1. |
| JUCE ohne Tracktion | Volle Kontrolle, aber Edit-Modell, Render, Aufnahme, PDC, Clips selbst bauen – Monate Mehraufwand. Bleibt Rückfall, falls der Spike scheitert. |

## Spike-Ergebnis (M0-06)
- Tracktion `develop` baut mit JUCE 9.0.3 ohne Rückfall; Build kalt ~5 min (GCC Release, 4 Kerne), warm ~1 min.
- Import WAV/MP3, Offline-Render (`RenderSpecification`) mit Lautheit (`LoudnessMeter`, ±0,1 LU/dB), 12 Eingänge über
  Hosted Device (0 fehlende Blöcke, −138 dB), eigenes VST3 (±0,01 dB) – alles headless ohne Display.
- CI (Windows 2025/MSVC, Ubuntu 24.04/GCC+Clang): alle Jobs grün; Build kalt 5–14 min je Job.
- Befunde: Tracktions Aufnahmepfad allokiert im Callback (RTSan-Strategie in M0-07); MP3 ohne Gapless (M2-01);
  WindowsMedia-Workaround nötig.

## Risiken
- Tracktion `develop` ist ungetaggt; Kompatibilität mit JUCE 9.0.3 unbelegt → erste Prüfung im Spike.
- Tracktions Out-of-Process-Scanner lagert nur VST/AU aus → für LV2/CLAP erweitern.
- JUCE kann kein CLAP, unter Linux keinen Screenreader und kein Wayland.

## Folgen
- Spike M0-06 nach `team/board/backlog/M0-06.md`; Ergebnis entscheidet JUCE-Version und Tracktion-Pin.
- Gate auf CMake (M0-07) übernimmt die Pins und Definitionen aus dem Spike.
