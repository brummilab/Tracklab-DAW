# Roadmap

Stand: 09.10.2026 · Quelle: [`team/plan/PLAN.md`](../team/plan/PLAN.md) und Auftrag §12 · Entscheidungen E4, E53, E54

✅ erledigt · 🔄 läuft · ⏳ geplant

```mermaid
flowchart LR
    M0["M0<br/>Start & Spike"]:::done --> M1["M1<br/>Fundament"]:::active
    M1 --> M2["M2<br/>Live-Mitschnitt"]:::planned
    M2 --> M3["M3<br/>Claude + MCP"]:::planned
    M3 --> M4["M4<br/>Spuren & Aufnahme"]:::planned
    M4 --> M5["M5<br/>Editing & Comping"]:::planned
    M5 --> M6["M6<br/>Mixer & Plugins"]:::planned
    M6 --> M7["M7<br/>Automation & MIDI"]:::planned
    M7 --> M8["M8<br/>Mastering & Export"]:::planned
    M8 --> M9["M9 = 1.0<br/>Interop & Politur"]:::release
    M9 --> M10["M10 ff.<br/>Rest des Katalogs"]:::later

    classDef done fill:#15803d,stroke:#15803d,color:#fff
    classDef active fill:#d97706,stroke:#d97706,color:#fff
    classDef planned fill:#374151,stroke:#6b7280,color:#fff
    classDef release fill:#1d4ed8,stroke:#1d4ed8,color:#fff
    classDef later fill:#fff,stroke:#6b7280,color:#374151,stroke-dasharray: 4 3
```

| | Meilenstein | Inhalt | Abnahme |
|---|---|---|---|
| ✅ | **M0** Start & Spike | Team-Prozess, Gate, Recherche, Tech-Stack (ADR-001), Engine-Spike JUCE 9 + Tracktion | Spike baut in der CI unter Windows und Linux |
| 🔄 | **M1** Fundament | Engine, Befehlsliste, Undo/Redo, Audio-Geräte ✅ · Projektformat `.tracklab` 🔄 · Autosave/Backups, Kommandozeile (`render`, `analyze`), erstes Fenster mit „Über Tracklab“, Test-Builds ⏳ | Projekt anlegen, speichern, öffnen; Test-Builds zum Herunterladen |
| ⏳ | **M2** Live-Mitschnitt (Workflow A) | Import Stereo und Mehrspur, Stille-Erkennung, Setlist-Import, Fades, LUFS/True-Peak-Meter, Normalisierung pro Song, Export WAV 48/24 + MP3 mit BWF-Zeitstempel | Mojo-Club-Mitschnitt wird in Songs zerlegt; Resolve übernimmt die Dateien ohne Nacharbeit |
| ⏳ | **M3** Claude + MCP | Claude-Panel (Plan-Vorschau, Undo pro Auftrag, Kosten), Songgrenzen per Claude, MCP-Server für Claude Code/Desktop | Workflow A per Claude-Befehl, dieselben Befehle über MCP |
| ⏳ | **M4** Spuren & Aufnahme | Spurtypen, Ordner, Busse, 12-Kanal-Aufnahme, Monitoring, Metronom, Punch, Loop-Takes, Latenzmessung | 12 Kanäle aufgenommen (Testprotokoll) |
| ⏳ | **M5** Editing & Comping | Lanes, Swipe-Comping, Razor-Editing, Crossfades, Clip-Gain, Ripple, Gruppen-Edit | Gesang aus 3 Takes gecomped; Drum-Edit phasenkohärent |
| ⏳ | **M6** Mixer & Plugins | Mischpult, Inserts, Sends, Sidechain, Latenzausgleich, VST3 mit abgeschottetem Scanner, Basis-Effekte, **Cue-Mixe** | Drum-Bus + Parallelkompression per Claude; Plugin-Absturz ohne Projektverlust; Cue-Mix pro Musiker |
| ⏳ | **M7** Automation & MIDI | Automationsspuren und -modi, Piano-Roll, Drum-Editor | Fader-Ride in Touch; MIDI-Spur bearbeitet |
| ⏳ | **M8** Mastering & Export | Mastering-Seite, Referenz-A/B, Dither, Render-Queue, Stems, Metadaten, **Mid/Side** | Workflow B (Bandaufnahme) Ende-zu-Ende exportiert, Messbericht korrekt |
| ⏳ | **M9 = 1.0** Interop & Politur | DAWproject, MIDI, LV2, CLAP, Reaper-Shortcuts, Screensets, helles Theme, Barrierefreiheit, Installer, **Handy-/Tablet-Fernbedienung** | Akzeptanztest A + B durch David |
| ⏳ | **M10 ff.** | Rest des Feature-Katalogs (v2, dann „später“): u. a. Stem-Separation, ARA, Spektral-Editing, Modulatoren, AAF/RPP-Import, Mastering-Assistent, MCU | je Meilenstein per Entscheidung |

Optimierungsrunden (Aufräumen, Geschwindigkeit) nach M2, M5 und M8.

## Wann du selbst testen kannst
1. **Nach M1-07 (Kommandozeile):** Handtest Audio-Geräte unter Windows und Linux.
2. **Nach M1-08/M1-09:** erstes Programmfenster und Test-Builds zum Herunterladen.
3. **Ende M2:** erster echter Arbeitsablauf – Konzertmitschnitt rein, fertige Songs raus.

## Was Tracklab einzigartig macht (E53)
- Claude ist eingebaut, jede Aktion geht auch per Claude oder MCP – mit Rückgängig und Bestätigung.
- Konzertmitschnitte werden fast automatisch in fertige Songs zerlegt.
- Open Source (AGPL), vollwertig unter Windows und Linux.
- Eigene Effekte und später DSP-Skripte.
