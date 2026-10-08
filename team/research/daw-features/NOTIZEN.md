# DAW-Funktionen – Notizen (M0-08, Teil 1)

Stand: 08.10.2026 · Lauf: `researcher` (abgebrochen bei Turn-Limit, Bericht mit Teilstand) · Übernommen vom Lead.
**Einschränkung:** reaper.fm, cockos.com, manual.ardour.org, steinberg.help, bitwig.com gesperrt. Primär gelesen nur
Ardour-Manual (GitHub `Ardour/manual` @ `9628c8b`) und Quellcode von Drittprojekten (SWS, reaper-keys, ReaScripts).
Kennzeichnung: **[P]** Primärquelle gelesen · **[S-H]** Snippet einer Herstellerseite · **[S]** sekundär.

## 1. †-Herkünfte (Auftrag §9, Zeilen in `docs/auftrag/Claude-Code-Prompt.md`)

| Zeile | Feature | Befund |
|---|---|---|
| 347, 565 | Screenreader | JUCE: nur Windows (E37). OSARA (Reaper) nur Windows/macOS [S-H] |
| 361 | Autosave/Backups (Reaper) | bestätigt [S] |
| 365 | Subprojekte (Reaper 5.11) | bestätigt [S] |
| 375 | Gruppen (Pro Tools/Cubase) | Cubase Link Groups bestätigt [S-H]; Pro Tools offen |
| 376 | VCA (Cubase/Pro Tools/Studio Pro) | alle bestätigt (Cubase nur Pro; Pro Tools Native seit 12.2) [S-H/S] |
| 394 | Latenz per Loopback (Reaper/Cubase) | bestätigt, aber manueller Ablauf (ReaInsert-Ping, Cubase Record Shift) → „halbautomatisch“ |
| 402 | Dynamic Split (Reaper) | bestätigt [S] |
| 403 | Razor Edits (Reaper 6.x) | bestätigt [S-H] |
| 407 | Clip-Gain (Pro Tools 10) | bestätigt [S-H] |
| 408 | Phasenkohärentes Gruppen-Editing | **Korrektur:** Cubase AudioWarp hält keine Phasenkohärenz → „Cubase Slice-Quantize/Audio Alignment, Pro Tools Beat Detective“ |
| 409 | AudioWarp (Cubase) | Name bestätigt; nicht für Mehrspur-Drums |
| 422 | Drum Maps (Cubase) | bestätigt [S-H] |
| 427 | MPE (Bitwig) | nur Marketing-Beleg → „Bitwig (Marketing)“ |
| 437 | Arranger-Spur | Cubase bestätigt; Studio Pro offen |
| 455 | Control Room/Cue-Mixe (Cubase) | bestätigt [S-H] |
| 464 | Trim-Automation (Pro Tools) | bestätigt, nur Ultimate [S] |
| 467 | Automation-Clips | Studio Pro **nicht belegt** → streichen; Bitwig offen |
| 478 | Presets/FX-Chains (Reaper) | offen (nicht erhoben) |
| 485 | ReaEQ/ReaComp/ReaGate | bestätigt [S-H] |
| 495 | LUFS-Meter (Reaper) | bestätigt als „Reaper 6.30 Master-Metering“ (LUFS-M/S/I, LRA) |
| 496 | Mastering-Seite (Studio Pro Project) | bestätigt [S] |
| 500 | DDP-Export | Studio Pro bestätigt; **Reaper nicht belegt** → streichen |
| 501 | ISRC/CD-Text (Studio Pro) | nur Forenbeleg → unbelegt |
| 511 | Stems, Bounce-in-Place (Reaper) | bestätigt [S-H Wiki] |
| 513 | LUFS-Normalisierung beim Render (Reaper 6.30/6.37) | bestätigt; „Plattform-Presets“ = Tracklab-eigen |
| 514 | Metadaten BWF/ID3/Vorbis (Reaper 6.10/6.11) | bestätigt [S] |
| 517 | AAF-Import | Pro Tools [S]; Ardour bestätigt (Fades, Volume, Pan) [P] |
| 528 | Screensets (Reaper) | bestätigt [S-H Wiki] |
| 531 | Logical Editor (Cubase) | bestätigt [S-H] |
| 539 | Freeze/Render-in-Place (Reaper) | bestätigt [S-H Wiki] |
| 540 | Anticipative FX (Reaper) | bestätigt (Render-ahead, Standard 200 ms) [S-H Wiki] |
| 547 | MCU/HUI | Reaper MCU bestätigt, HUI teilweise; Ardour MCU [P] |
| 548 | OSC | Ardour bestätigt [P]; Reaper ohne Handbuchbeleg |

## 2. Reaper-Referenzverhalten (alles [S])
- **Razor Edits:** mehrspurige Bereiche inkl. Envelopes; Alt+Rechts-Drag; Shift ergänzt; Ctrl+Drag kopiert; Ripple-Delete
  möglich; in Fixed Lanes Comping-Button.
- **Fixed Item Lanes (Reaper 7):** bis 128 Lanes je Spur; „Play only lane“/„Play all lanes“; Aufnahme je Durchgang in
  neue Lane; Comping per „Comp into new empty lane“ und Links-Drag.
- **Ripple:** aus / pro Spur / alle Spuren; seit 7.35 Toolbar schaltet letzten Modus, Wechsel per Rechtsklick.
- **Region Render Matrix:** Regionen × Spuren, jede Zelle = Stem dieser Spur für diese Region.
- **Wildcards (belegt):** `$region`, `$regionnumber`, `$track`, `$marker`, `$namecount`, `$timelineorder[000]`,
  `$filename`, `$author`; `/` erzeugt Unterordner. Nicht belegt: `$project`, `$tracknumber`, `$notes` (Auftrag Z. 367 „7.11“).
- **Empfehlung MVP:** Razor-Bereiche, Fixed Lanes mit Swipe-Comping, Ripple aus/Spur/alle, Render pro Region mit
  Reaper-kompatiblen Wildcards; Render-Matrix ab v1.

## 3. `reaper-kb.ini` (aus Drittquellcode [P], kein Cockos-Dokument)
- `KEY <mod> <taste> <command> <section>` – Modifier = Windows-Accelerator-Flags: +1 virtuelle Taste, +4 Shift, +8 Ctrl,
  +16 Alt (Ctrl = 9, Alt = 17, Shift = 5, Ctrl+Shift = 13 …); Taste = Windows-VK-Code (Pfeile 32805–32808, Entf 32814 …);
  Section 0 = Main, 32060 = MIDI-Editor.
- `ACT <flags> <section> "<id>" "<name>" <cmd> …` (Custom Actions), `SCR <flags> <section> <id> "<beschreibung>" "<pfad>"`.
- `[VERIFIZIEREN]`: Flag-Bits von ACT/SCR, Super-Taste, weitere Section-IDs, Kodierung.
- **Standard-Shortcuts:** nicht belegt erstellbar (Default-Liste gesperrt; Quellen widersprechen sich bei R/Ctrl+R) →
  aus einem Export einer **Standardinstallation** ableiten (F11).

## 4. Ardour [P]
- Snapshots: Varianten der Sitzungsdatei im selben Ordner, gleiche Audiodaten (Vorbild für M2).
- Region FX (Ardour 9): Effekt + Automation hängen am Clip, offline gerendert (Vorbild M5).
- Cue-Marker: für MVP irrelevant. AAF-Import „best effort“ (Fades, Volume, Pan) – Priorität v2 passt.
- Mixer-Strip-Templates (Trim, Plugin-Parameter, Fader) belegt.

## Folgen (Lead)
- Herkunftsspalte in §9 korrigiert über DESIGN Rev 2 „Korrekturen am Auftrag“ (Punkt 8); Features bleiben.
- F11 präzisiert: `reaper-kb.ini` einer Standardinstallation + eigene Belegung.
