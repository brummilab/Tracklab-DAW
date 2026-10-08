# Lautheits-Standards – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
**Einschränkung:** tech.ebu.ch, itu.int, artists.spotify.com, support.google.com, support.deezer.com waren gesperrt –
Primärquellen nur über Such-Snippets (URL angegeben), kein Volltext. Offene Punkte `[VERIFIZIEREN]`.

## 1. Versionen
- **ITU-R BS.1770-5** (22.11.2023, in Kraft) – https://www.itu.int/rec/R-REC-BS.1770-5-202311-I. Revisionsentwurf am
  28.08.2026 eingereicht → Nachfolger möglich. Unterschiede zu -4 `[VERIFIZIEREN]`.
- **EBU R128 v5.0** (Nov. 2023) – https://tech-qual.ebu.ch/files/live/sites/tech/files/shared/r/r128v5_0.pdf.
  Eckwerte: −23 LUFS ±0,5 LU (Live ±1 LU), max. −1 dBTP.
- **Tech 3341 v4.0** (Nov. 2023, inhaltlich = v3.0 von 2016) – https://tech.ebu.ch/publications/tech3341/changelog.
- **Tech 3342**: aktuelle Version `[VERIFIZIEREN]`.
- **EBU Loudness Test Set v5.0** – https://tech.ebu.ch/publications/ebu_loudness_test_set.

## 2. Toleranzen und Testsignale
- Indirekt (Test von `sdroege/ebur128`, `tests/reference_tests.rs`): M/S/I ±0,1 LU; True Peak +0,2/−0,4 dB
  (`seq-3341-15…23`); LRA ±1 LU. Gegen Tech 3341 prüfen `[VERIFIZIEREN]`.
- **Lizenz der EBU-Testsignale nicht gefunden** → nicht ins Repo (R13). Optionen: CI-Download mit SHA-256-Prüfung
  (wenn Terms erlauben) oder eigene Signale nach Tech-3341-Beschreibung synthetisieren.

## 3. Algorithmus (belegt aus libebur128 v1.2.6, `ebur128.c`)
- Absolutes Gate −70 LUFS, relatives Gate −10 LU; Blöcke 400 ms, 75 % Überlappung.
- True Peak: Polyphase-FIR 49 Taps, 4× unter 96 kHz, 2× unter 192 kHz.
- LRA: Short-term (3 s), 10.–95. Perzentil, relatives Gate −20 LU.

## 4. Plattform-Presets (Auftrag §10.2)

| Preset | Befund | Status |
|---|---|---|
| Spotify | −14 LUFS (Normal), Loud −11, Quiet −19; 1 dB Headroom für Lossy (artists.spotify.com, Snippet). „−2 dBTP bei lauten Masters“ nur in Drittquellen | teils belegt |
| Apple Music | −16 LUFS **nicht** von Apple belegt; Sound Check nur absenkend | Community-Wert |
| YouTube | −14 LUFS nur Drittquellen | Community-Wert |
| Amazon Music | −14 LUFS; TP −1 vs. −2 dBTP widersprüchlich → −2 konservativ | Community-Wert |
| Tidal | −14 LUFS, Album-Normalisierung, kein Anheben (Drittquellen, AES-Paper 2019) | Community-Wert |
| Deezer | −15 LUFS nur Drittquellen; offizielle Hilfe nennt keinen Wert | Community-Wert |
| **SoundCloud (neu)** | −14 LUFS, TP < −1 dBTP, bei lauteren Masters < −2 dBTP (help.soundcloud.com, Snippet) | belegt |
| Podcast (AES TD1008, 2021) | Sprache ≈ −18 LUFS, Musik ≤ −16 LUFS; TP-Wert `[VERIFIZIEREN]` | Sekundär |
| EBU R128 | −23 LUFS ±0,5 LU, −1 dBTP | belegt |
| Video-Pipeline | eigenes Preset −14 LUFS/−1 dBTP, WAV 48/24 – Vault-Abgleich offen (kein Vault-Zugriff) | intern |

## 5. Dither (Literatur, nicht frisch belegt)
- TPDF ±1 LSB als Standard bei Reduktion auf 16 Bit, letzte Stufe vor dem Encoder; Noise Shaping optional (aus);
  kein Dither bei 24 Bit. Referenz: Lipshitz/Vanderkooy/Wannamaker, JAES 1992.

## 6. Test-Orakel
- **libebur128** (C, MIT, v1.2.6 vom 14.02.2021) – https://github.com/jiixyj/libebur128
- **sdroege/ebur128** (Rust, MIT, v0.1.10) – Testdatei als Vorlage für Dateinamen/Toleranzen.
- Beide BS.1770-4; Auswirkung von -5 auf Stereo `[VERIFIZIEREN]`.

## Folgen für den Lead (DESIGN Rev 2 / Auftrag-Korrekturen)
- §10.2: nicht offizielle Werte als „Community-Wert“ kennzeichnen, SoundCloud und Podcast (TD1008) ergänzen, Default
  bleibt „Kein Ziel – nur messen“.
- Normstand im Code dokumentieren (BS.1770-5, R128 v5.0, Tech 3341 v4.0).
- True-Peak-Oversampling 4×/2× wie libebur128 (Implementer-Entscheidung).

## Offen für den PO (→ `TODO-PO.md`)
- **F28** EBU-PDFs und Terms der Testsignale manuell bereitstellen oder Domains freigeben; Nutzung der Testsignale in CI.
