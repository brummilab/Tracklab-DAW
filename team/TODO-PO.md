# TODO für David (Product Owner)

Hier stehen nur Entscheidungen und Prüfaufgaben. Antwort direkt unter die Frage schreiben
(`**Antwort:** …`) oder im Chat. Ergebnisse werden zu E-Einträgen in `ENTSCHEIDUNGEN.md`.

---

## Blocker aus Session 1 (08.10.2026)

### F0 – Agent-Team-Vorlage bereitstellen
Kein Zugriff auf `agent-team-vorlage` aus dieser Session (GitHub-App nicht für die
Organisation freigegeben bzw. Repo nicht sichtbar). Ohne Vorlage kann ich §3.1 (Prozessvertrag,
Agents, Gate-Gerüst) nicht übernehmen.
- **Empfehlung:** Claude-GitHub-App für die Organisation bzw. das Repo freigeben, damit ich es per
  `add_repo` einbinden kann. Alternativ: Inhalt von `vorlage/` als ZIP in den Chat hochladen.
- **Default ohne Antwort:** nichts – die Agents (`.claude/agents/`) lege ich erst mit der Vorlage an.
- **Antwort:**

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

---

## Entscheidungsrunde 1

### F1 – Repo-Ort und Sichtbarkeit
Das Repo existiert bereits als **`brummilab/Tracklab-DAW`, privat** (statt vorgeschlagen `brummilab/tracklab`).
- **Empfehlung:** so lassen (privat, bestehender Name). Öffentlich erst nach Markenprüfung „Tracklab“.
- **Default:** `brummilab/Tracklab-DAW`, privat.
- **Antwort:**

### F2 – Lizenz
- **Empfehlung:** AGPLv3 (Folge aus JUCE AGPLv3 + Tracktion Engine GPLv3; Auftrag §5). Closed Source
  bräuchte kommerzielle Lizenzen für beide.
- **Default:** AGPLv3, `LICENSE` wird ersetzt.
- **Antwort:**

### F3 – Tech-Stack (ADR-001)
C++20/CMake, JUCE 8, Tracktion Engine 3.x, VST3 (MVP), LV2 + CLAP (v1), ASIO/WASAPI/ALSA/JACK,
nlohmann/json; Alternativen Ardour-Fork, Rust-Stack, JUCE ohne Tracktion (`DESIGN.md` §2).
- **Empfehlung:** Vorschlag annehmen **unter Vorbehalt des Engine-Spikes** (M0-06).
- **Default:** Spike starten, ADR-001 nach Spike-Ergebnis zur Bestätigung vorlegen.
- **Antwort:**

### F4 – Reihenfolge
- **Empfehlung:** Workflow A zuerst (M1 → M2 Mitschnitt ohne Claude → M3 Claude + MCP), danach Recording und Mixing.
- **Default:** wie empfohlen.
- **Antwort:**

### F5 – Standardmodell im Claude-Panel
- **Empfehlung:** `claude-sonnet-5-5`; Opus für Analyse/Planung, Haiku für Ein-Schritt-Befehle.
- **Default:** wie empfohlen.
- **Antwort:**

### F6 – Windows-Installer
- **Empfehlung:** Inno Setup (einfaches Skriptformat, verbreitet; endgültig nach `research/ci-packaging/`).
- **Alternative:** NSIS.
- **Default:** Inno Setup.
- **Antwort:**

### F7 – Release-Schema
- **Empfehlung:** `vJJJJ.MM.N` wie in meinen anderen Projekten, Releases gebündelt.
- **Default:** bestätigt.
- **Antwort:**

### F8 – Agent Teams (experimentell) aktivieren?
- **Empfehlung:** nein (Grenzen bei Resume, Koordination, Shutdown; Mailbox nicht im Repo).
- **Default:** nein.
- **Antwort:**

### F9 – Info: Audio-Interface der Band
Modell, Anzahl Eingänge, Treiber (ASIO unter Windows? class-compliant unter Linux?).
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
- **Hinweis:** In dieser Cloud-Session fehlen Obsidian-MCP und Zugriff auf die Vorlage (F0, F0b).
- **Antwort:**

### F13 – Push-Weg in Cloud-Sessions
Die Cloud-Session ist auf den Arbeitsbranch `claude/new-session-mymk20` konfiguriert; laut R3 pushe ich
zusätzlich direkt auf `main` (Fast-Forward, kein PR).
- **Empfehlung:** so beibehalten.
- **Default:** beide pushen, `main` ist führend.
- **Antwort:**

---

## Prüfaufgaben (V)
– noch keine
