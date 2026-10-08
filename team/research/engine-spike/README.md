# Engine-Spike (Pflicht vor Annahme von ADR-001)

## Ziel
Minimal-App mit JUCE + Tracktion Engine, gebaut in der CI für Windows und Linux:
1. Stereo-WAV und -MP3 importieren
2. eine Region offline rendern und ihre Lautheit messen (Workflow A)
3. 12 Eingänge aufnehmen (Dummy-Device in CI)
4. ein VST3 laden
Ergebnis: Bericht hier, Build-Zeiten, Probleme, Empfehlung für ADR-001.

## Fragen
- Aktuelle Versionen und Commits: JUCE 8.x, Tracktion Engine 3.x `[VERIFIZIEREN]`
- Einbindung: Submodule oder CMake FetchContent mit festen Commits?
- MP3-Dekodierung unter Windows/Linux in JUCE (Lizenz, Plattform-Codecs)
- Dummy-/Null-Audio-Device für Headless-CI
- Build-Zeit und ccache/sccache-Caching auf GitHub-Runnern

## Quellen
- https://github.com/juce-framework/JUCE
- https://github.com/Tracktion/tracktion_engine (Examples, Tests)
- https://docs.juce.com/
