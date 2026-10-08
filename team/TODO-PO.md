# TODO für David (Product Owner)

Nur **Entscheidungen** und **Reviews/Prüfungen**. Antwort direkt unter die Frage schreiben (`**Antwort:** …`) oder
im Chat. Der Lead überträgt sie nach `ENTSCHEIDUNGEN.md` und streicht den Punkt.

Erledigt am 08.10.2026: Entscheidungsrunden 1 und 2 mit „Defaults ok“ (E1–E8, E13–E21, E23–E38). Offen sind nur noch
Infos und Zugänge ohne Default.

---

## Offen

### F0b – Vault-Zugriff und Vault-Sync für Cloud-Sessions
In dieser Cloud-Session ist kein Obsidian-MCP-Server verbunden; den Vault konnte ich weder lesen (R2)
noch die Abschnitte *Status*/*Offene Punkte* patchen (R5). `Projektinhalt.md` im Repo ist deshalb
aus dem Auftrag aufgebaut. Der Auftrag (`docs/auftrag/Claude-Code-Prompt.md`) liegt im Repo.
- **Bitte:** (a) Obsidian-MCP-Server für Cloud-Sessions verfügbar machen **oder** künftig lokal am
  Laptop arbeiten; (b) Vault-Sync nach Vorbild: ein anderes eigenes Projekt einrichten – maßgeblich ist deine Notiz
  `Vault-Sync-Notiz (Git-Hook)`. Für den lokalen Clone auf dem Laptop:
  ```powershell
  cd C:\temp
  git clone https://github.com/brummilab/Tracklab-DAW.git
  cd Tracklab-DAW
  git config vault.note "<Vault>/Tracklab/Projektinhalt.md"
  ```
  Danach post-merge-Hook und geplanten Pull genau wie in meinen anderen Projekten anlegen (kein repo-lokales
  `core.hooksPath`, R6).
- **Zusätzlich bitte:** den heutigen Stand in der Vault-Notiz nachtragen – Abschnitt *Status*:
  „08.10.2026: M0 begonnen, Grundgerüst im Repo brummilab/Tracklab-DAW, warte auf Entscheidungsrunde 1“;
  *Offene Punkte*: F0, F0b, Entscheidungsrunde 1. (Oder ich mache es, sobald der MCP-Zugriff steht.)
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
- **Antwort:**

### F12 – Info: Entwicklung lokal oder in Cloud-Sessions?
Wichtig für Vault-Sync (lokal: post-commit-Hook; Cloud: post-merge + geplanter Pull) und Tag-Push
(Cloud: 403 → du setzt Tags).
- **Hinweis:** In dieser Cloud-Session fehlt der Obsidian-MCP (F0b).
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
