# TODO für David (Product Owner)

Nur **Entscheidungen** und **Reviews/Prüfungen**. Antwort direkt unter die Frage schreiben (`**Antwort:** …`) oder
im Chat. Der Lead überträgt sie nach `ENTSCHEIDUNGEN.md` und streicht den Punkt.

Erledigt am 08.10.2026: Entscheidungsrunden 1 und 2 mit „Defaults ok“ (E1–E8, E13–E21, E23–E38). Offen sind nur noch
Infos und Zugänge ohne Default.

---

## Offen

### F45 – Eigene Plugins erstellen (PO-Wunsch 09.10.2026, Beispiel Goodhertz Vulf Compressor)
**Fremde Plugins nutzen** ist schon geplant: VST3-Hosting mit Sandbox in M6 (CLAP/LV2 später). Vulf Comp gibt es laut
goodhertz.com/faq (Stand 09.10.2026) als VST3/AAX für Windows und AU/AAX/VST3 für macOS – **nicht für Linux**. Unter
Windows läuft es also in Tracklab, unter Linux nicht.
**Eigene Plugins erstellen** – drei Wege, kombinierbar:
- **A) Eigene VST3/CLAP-Plugins mit JUCE** in einem eigenen Repo (z. B. `tracklab-fx`): laufen in Tracklab *und* in
  jeder anderen DAW, Linux + Windows. Entwicklung mit Claude Code wie hier (Agent-Team, Tests, Golden-Render).
- **B) Tracklab-Effekte eingebaut** (Kompressor, EQ, Sättigung …), Teil der App, steuerbar über Claude/MCP.
- **C) DSP-Skripte in Tracklab** (wie Reapers JSFX): Effekt als kurzer Code, sofort ohne Neustart hörbar; Claude kann
  ihn auf Zuruf schreiben („bau mir einen pumpenden Lo-Fi-Kompressor“). Braucht Skriptsprache/Compiler (z. B. FAUST)
  und Sandbox – neue Sicherheitsfrage, weil Claude dann Code erzeugt, der im Audio-Pfad läuft.
- Rechtlich: eigene Effekte dürfen *klanglich inspiriert* sein, aber keine Namen, Logos oder Oberflächen kopieren.
- **Default:** A als eigenes Projekt nach M1 starten (erstes Plugin: „Lo-Fi-Kompressor“ inspiriert von Vulf), B wie
  geplant in M6, C als Recherche-Karte R-01 (FAUST & Co., Sicherheit) mit Entscheidung vor v2.
- **Antwort:**

### F44 – Undo: Historienverlust in einem Sonderfall (M1-03), vorerst hinnehmen?
JUCE hat einen Fehler im Undo-Zwischenspeicher. Ohne Eingriff in JUCE ist ein gescheiterter Befehl nur sicher
zurückzunehmen, wenn die **ganze Undo-Historie** verworfen wird – und zwar (Review Runde 2) **in jeder Sitzung, sobald
einmal „Rückgängig, dann neue Änderung“ passiert ist**, bei jedem später mitten im Schreiben scheiternden Befehl (typisch:
ein Claude-Auftrag, dessen späterer Schritt ungültig ist). Das Projekt bleibt korrekt, nur „Rückgängig“ ist danach leer.
- **Default:** M1-03 so mergen (sicher, keine falschen Wiederholen-Schritte); O-09 **direkt nach M1** statt vor M3:
  JUCE-Fehler melden, kleiner Patch (idempotent beim Konfigurieren) und Claude-Aufträge vorab vollständig prüfen.
- Alternative: jetzt schon JUCE patchen (Eingriff in Fremdcode, mehr Pflegeaufwand).
- **Antwort:**


### F9 – Info: Audio-Interface der Band
Modell, Anzahl Eingänge, Treiber (ASIO unter Windows? class-compliant unter Linux?).
Wichtig für F36: Kanalnamen, Pro-Audio-Profil unter PipeWire und ASIO-Treiber hängen am Modell.
- **Antwort:**

### F10 – Info: welcher Reaper-MCP-Server läuft heute?
Name/Repo – als Designreferenz für `research/mcp-server/`.
- **Antwort:**

### F11 – Info: Reaper-Shortcuts und Custom Actions
Bitte `reaper-kb.ini` exportieren und lokal bereitlegen (nicht ins Repo, falls Persönliches drin ist) –
Grundlage für das Reaper-kompatible Shortcut-Preset.
**Ergänzt (M0-08):** Die Reaper-Standardbelegung ist aus der Cloud nicht belegbar (reaper.fm gesperrt, Quellen
widersprüchlich). Ideal: zwei Exporte – (a) `reaper-kb.ini` einer **frischen Standardinstallation** (Actions → Key map →
Export), (b) deine eigene Belegung. Optional `whatsnew.txt` aus dem Reaper-Programmordner.
- **Antwort:**

### F12 – Info: Entwicklung lokal oder in Cloud-Sessions?
Wichtig für Vault-Sync (lokal: post-commit-Hook; Cloud: post-merge + geplanter Pull) und Tag-Push
(Cloud: 403 → du setzt Tags).
- **Antwort:**

### F22 – Test mit echtem API-Key
Einige Punkte (strict + deferred Tools, Cache-Treffer, SSE-Verhalten) lassen sich nur mit einem Key prüfen.
- **Bitte:** später einen Test-Key mit Ausgabenlimit bereitstellen – **nicht ins Repo**, nur als Umgebungsvariable
  bzw. GitHub-Secret. Wann, entscheidest du; ohne Key bleiben die Punkte `[VERIFIZIEREN]`.
- **Antwort:**

### F28b – EBU-Primärquellen (optional, aus F28)
Default läuft (eigene Testsignale). Für die Validierung gegen das offizielle Set wäre hilfreich: Domains `tech.ebu.ch`
und `itu.int` in der Cloud-Umgebung freigeben **oder** `tech3341.pdf`, `tech3342.pdf`, `r128v5_0.pdf` und die
Nutzungsbedingungen des „EBU Loudness Test Set“ lokal ablegen (nicht ins Repo).
- **Antwort:**

---

## Prüfaufgaben (V)
– noch keine
