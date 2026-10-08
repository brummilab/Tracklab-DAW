# Songgrenzen-Erkennung – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
Einschränkung: Fachliteratur (Foote 2000, MSAF, Scheirer/Slaney, Saunders) und essentia.upf.edu nicht abrufbar →
Verfahrensaussagen aus Fachwissen, `[VERIFIZIEREN]`. Lizenzen teils direkt aus Repos (raw.githubusercontent.com) belegt.

## Kontext (Auftrag)
- Workflow A: Stereo-Mitschnitt, 2 Sets, ~19 Songs; Setlist optional; Vorschlag → Nutzer korrigiert
  (`docs/auftrag/Claude-Code-Prompt.md:177-179`, Tool `analyze.find_song_boundaries` `:298`, Fixture `:611-616`).
- Offline-Job im Worker-Thread mit Abbruch/Fortschritt – kein Audio-Thread.

## 1. Verfahren (Fachwissen)
- Pegel/Stille: RMS in 100-ms-Blöcken, **relative** Schwelle (Perzentil des Gesamtpegels), Raumrauschen beachten.
- Applaus: hohe spektrale Flachheit + hohe Energie + hohe ZCR; Ansage: mittlerer Pegel, 4-Hz-Modulation.
- Novelty nach Foote (ICME 2000): Selbstähnlichkeitsmatrix, Schachbrett-Kernel 20–60 s → Songwechsel ohne Pause.
- Bekannte Anzahl K: dynamische Programmierung über Kandidaten mit Längen-Constraints, O(K·N²).
- MSAF-Algorithmen auf Studio-Pop getrimmt → für Mitschnitte nur bedingt geeignet.

## 2. Parameter-Defaults (Vorschlag, zu tunen)
- Mindestlänge 60 s (einstellbar bis 30 s), Höchstlänge 12 min; Pause ≥ 3 s deutlich unter Song-Median oder Applaus ≥ 2 s.
- Polster 1 s vor, 3 s nach dem Song (nie in die Nachbarpause); Snap ±250 ms auf Pegel-Minimum.
- Ansagen bei Pausen > 8 s als Lücke, nicht als Song. Setpause (> 5 min Stille) als Hartkriterium.

## 3. Evaluation
- Boundary-F-Measure mit Fenster ±0,5 s und ±3 s (MIREX); belegt in `mir_eval/segment.py` `detection(...)`.
  In C++ eigene Mini-Implementierung; mir_eval nur einmalig als Referenz, nicht im Gate.
- **Fixture `mitschnitt-mini`** (generiert, fester Seed, R13): Stereo 44,1 kHz, 3–4 min; 3 „Songs“ (Akkorde + Rauschbursts,
  verschiedene Tonart/Tempo), Applaus (gefiltertes Rauschen mit Poisson-Klicks, 4–8 s), Ansage (Rauschen/Sinus mit
  4-Hz-AM, 5 s), Raumrauschen −60 dBFS; Ground Truth als JSON; Varianten: bekannte/unbekannte Anzahl, nahtloser Übergang.
- Startziele: Basis ±0,5 s F = 1,0; schwere Variante ±3 s F ≥ 0,9.

## 4. Lizenzen

| Library | Lizenz | Beleg | Bewertung |
|---|---|---|---|
| Essentia | AGPLv3 / kommerziell; Modelle CC BY-NC-ND | Websuche `[VERIFIZIEREN]` | kompatibel, aber schwer – nicht nötig |
| aubio | GPL-3.0-or-later | Paketmetadaten `[VERIFIZIEREN]` | kompatibel, nicht für Segmentierung gebaut |
| Gist | GPLv3+ | Repo-README | kompatibel |
| libxtract | zlib | Repo-LICENSE | kompatibel |
| librosa | ISC (Python) | Repo-LICENSE | nur Referenz |
| madmom | Code BSD-artig, **Modelle CC BY-NC-SA** | Repo-LICENSE | Modelle **nicht** verwenden |
| mir_eval, MSAF | vermutlich MIT `[VERIFIZIEREN]` | – | nur Evaluation |

## 5. MVP-Empfehlung
Eigenbau (~500–800 Zeilen), nur JUCE-FFT: Dekodieren → Mono, 22,05 kHz → Merkmale je 100 ms (RMS dB, Flachheit,
Schwerpunkt, ZCR, Chroma/MFCC) → Block-Klassifikation {Stille, Applaus, Musik, Ansage} mit Hysterese/Medianfilter →
Kandidaten (Pausen-/Applausmitten + Novelty-Maxima) → Auswahl (ohne Setlist: Schwelle + Längenprüfung; mit Setlist: DP für
genau K Segmente) → Regionen {start, end, Konfidenz, Grenztyp}. Kann früh als CLI `analyze` kommen (M1), Claude-Nutzung in M3.

## Offen für den PO (→ `TODO-PO.md`)
- **F31** Eigenbau statt Fremdlibrary (Empfehlung).
- **F32** Defaults (60 s / 12 min / 1 s + 3 s) und kürzester/längster Song im Repertoire; Akzeptanzziel F ≥ 0,9 bei ±3 s.
- **F33** Lokale echte Mitschnitte mit handgesetzten Grenzen (CSV) zum Tunen – außerhalb des Repos.
