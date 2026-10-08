# Plan: Meilensteine

Status: ✅ erledigt · 🔄 in Arbeit · ⏳ geplant · ❓ wartet auf Entscheidung

Details zu Zielen und Akzeptanz: Auftrag §12, `team/design/DESIGN.md` Abschnitt 9.

| M | Ziel | Status | Abnahme |
|---|---|---|---|
| M0 | Vorlage übernommen, Gate auf C++ umgebaut, Agents ergänzt, Recherche, ADR-001, Engine-Spike | ✅ 08.10.2026 | Entscheidungen E0–E39 ✅; Recherche ✅; Spike in CI Windows + Linux ✅; ADR-001 angenommen ✅; Gate C++/CMake + RTSan ✅ (Rückblick `team/RUECKBLICK-M0.md`; Reaper-Preset M0-08 → M9) |
| M1 | Fundament: CMake, JUCE + Tracktion, Audio-I/O, Projektformat, Autosave, Command-Registry v1, Undo, CLI (`render`, `analyze`), CI + Artefakte, „Über Tracklab“ mit Lizenz/Quelltext (E23) | 🔄 | Projekt anlegen/speichern/öffnen; Fixture-Render golden; Artefakte downloadbar |
| M2 | Mitschnitt-Workflow ohne Claude | ⏳ | Workflow A von Hand durchgespielt; Export passt in die Video-Pipeline |
| M3 | Claude-Integration MVP + MCP-Server | ⏳ | Workflow A per Claude-Befehl; dieselben Befehle über Claude Code via MCP |
| M4 | Spuren & Aufnahme | ⏳ | David nimmt 12 Kanäle auf (Testprotokoll) |
| M5 | Editing & Comping | ⏳ | Gesang aus 3 Takes gecomped; Drum-Edit phasenkohärent |
| M6 | Mixer & Plugins (VST3, Scanner-Isolation, Absturzerkennung; Sandbox nach Latenz-Spike, E34) | ⏳ | Drum-Bus + Parallelkompression per Claude; Plugin-Crash ohne Projektverlust |
| M7 | Automation & MIDI-Basis | ⏳ | Fader-Ride in Touch; MIDI-Spur bearbeitet |
| M8 | Mastering & Export komplett | ⏳ | Workflow B Ende-zu-Ende exportiert, Messbericht korrekt |
| M9 = 1.0 | Interop & Politur | ⏳ | Akzeptanztest A + B durch David |

Optimierungsrunden (O-Karten) nach M2, M5 und M8. Rückblick `team/RUECKBLICK-<M>.md` nach jedem Meilenstein.

Zusätzlich vor M6: Karte **Sandbox-Latenz-Spike** (E34). LV2/CLAP in M9 (E35).
