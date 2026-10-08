# Lizenz und Name – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead. **Keine Rechtsberatung.**
Einschränkung: gnu.org, juce.com, EUIPO, TMview waren gesperrt; Belege aus Original-Repos (geklont) und SPDX-Lizenztexten.

## 1. AGPLv3-Folge – belegt
- JUCE-Module: „dual-licensed under the AGPLv3 and the commercial JUCE licence“ (`LICENSE.md`, Tags 8.0.12 und 9.0.3).
- Tracktion Engine: „dual GPL3 (or later)/Commercial license“, letzter Tag `v3.2.0`; README: eigene JUCE- **und**
  Tracktion-Lizenz nötig für Closed Source.
- AGPLv3 §13 Abs. 2 erlaubt die Kombination mit GPLv3-Werken (SPDX `AGPL-3.0-only.txt`) → Gesamtwerk Tracklab unter
  **AGPLv3**. Empfehlung SPDX: `AGPL-3.0-only` (JUCE ohne „or later“).
- **Auftrag §5 veraltet:** JUCE 9 ist erschienen (`9.0.0` am 21.07.2026, `9.0.3` am 28.09.2026; parallel `8.0.15`).
  AGPL-Option unverändert. Tracktion `master` pinnt JUCE 8.0.14 + 17 Commits; JUCE-9-Kompatibilität im Spike klären.
- JUCE 9.0.3 enthält kein CLAP-Modul.
- Starter/Indie/Pro-Stufen nur sekundär belegt; für AGPL-Betrieb irrelevant.

## 2. Kompatibilität der Abhängigkeiten

| Baustein | Lizenz | Einstufung |
|---|---|---|
| VST3-SDK | MIT (steinbergmedia/vst3sdk, © 2026) | kompatibel |
| CLAP | MIT | kompatibel |
| ASIO-SDK | Steinberg proprietär **oder GPLv3** (JUCE `native/asio/LICENSE.txt`) | kompatibel mit GPLv3-Option → in `NOTICE`/ADR festhalten |
| LV2-Header / lilv | ISC | kompatibel (JUCE hostet LV2 ohne lilv) |
| nlohmann/json | MIT | kompatibel |
| Catch2 / GoogleTest | BSL-1.0 / BSD-3-Clause | kompatibel |
| libsamplerate, FLAC, Ogg, zlib, HarfBuzz | BSD/zlib/MIT | kompatibel |
| MP3-Dekoder (JUCE `MP3AudioFormat`) | JUCE-Lizenz, Dekoder ohne Encoder, `JUCE_USE_MP3AUDIOFORMAT` | unproblematisch |
| MP3-Encoder (JUCE `LAMEEncoderAudioFormat`) | ruft externes `lame` auf; LAME vermutlich LGPL `[VERIFIZIEREN]` | ADR: bündeln vs. extern |
| minimp3 | CC0 | kompatibel, evtl. unnötig |
| Inter | SIL OFL 1.1 | siehe 5 |

## 3. Pflichten
- Privates Repo / Eigengebrauch: kein „conveying“ (GPLv3 §0/§2) → keine Pflichten.
- **Builds an Bandmitglieder = conveying:** vollständigen Quelltext (inkl. Build-Skripte) mitgeben oder anbieten,
  Lizenztext + Hinweise aller Abhängigkeiten beilegen. Repo muss dafür nicht öffentlich sein; Empfänger dürfen weitergeben.
- AGPL §13 beim lokalen MCP-Server (localhost/stdio): vermutlich keine „remote“-Interaktion `[VERIFIZIEREN]`.
  Praxis: Menüpunkt „Über Tracklab → Quelltext“ mit Link deckt §13 und „Appropriate Legal Notices“ ab.
- Beiträge Dritter: später DCO oder CLA festlegen.

## 4. Marke „Tracklab“ (nur Websuche, Register nicht abfragbar)
- **2Simple „Tracklab“**: Mehrspur-Musikwerkzeug in Purple Mash (Schulplattform), Blog 03.05.2026 – **identischer
  Name, gleiche Domäne**, kritischster Treffer. Registrierung offen.
- „Track Lab“: VR-Musikspiel von Little Chicken (PSVR, 2018), nicht von Sony; US-Marke „TRACK LAB“ Kl. 9 (86398801) gelistet.
- Tracklib: Sample-Lizenzdienst, US-Marken 79295900, 88651340 – ähnlich, anderes Produkt.
- Keine Desktop-DAW „Tracklab“ gefunden.
- Empfehlung: privat unkritisch; vor Veröffentlichung EUIPO/TMview/DPMA/USPTO/WIPO (Kl. 9, 41, 42) prüfen, Plan-B-Name.

## 5. Inter im Logo
- OFL 1.1: Logo/GUI-Nutzung erlaubt; bei Auslieferung OFL-Text + Copyright beilegen. Wortbild-Logo als Pfade = Bild.

## Offen für den PO (→ `TODO-PO.md`)
- F2 Lizenz: Empfehlung `AGPL-3.0-only`.
- **F23** Quelltext-Angebot in der App („Über Tracklab“) als Akzeptanzkriterium in M1.
- **F24** JUCE 8.0.x oder 9.0.x – nach Spike in ADR-001 (Auftrag §5 korrigieren).
- **F25** Marke: privat behalten, vor Veröffentlichung Registerrecherche (nur PO mit Browserzugang).
- MP3-Encoder (LAME bündeln vs. extern) → ADR später.
