# TODO für David (Product Owner)

Nur **Entscheidungen** und **Reviews/Prüfungen**. Antwort direkt unter die Frage schreiben (`**Antwort:** …`) oder
im Chat. Der Lead überträgt sie nach `ENTSCHEIDUNGEN.md` und streicht den Punkt.

Erledigt am 08.10.2026: Entscheidungsrunden 1 und 2 mit „Defaults ok“ (E1–E8, E13–E21, E23–E38). Offen sind nur noch
Infos und Zugänge ohne Default.

---

## Offen

### F43 – Vor dem Umstellen auf öffentlich (E45)
Geprüft vom Lead (aktueller Stand + **gesamte Git-Historie**): keine Secrets/Tokens, keine Audiodateien, Commit-Adressen
nur `noreply` (Claude, GitHub, `brummilab@users.noreply`). Öffentlich würden aber:
- **a) Arbeitgeber-Bezug:** `agent-team-vorlage` wird in `docs/auftrag/Claude-Code-Prompt.md`, `team/README.md`,
  `team/TODO-PO.md`, `team/RESUME.md`, `Projektinhalt.md`, `CLAUDE.md`-Historie genannt. Prozessvertrag und
  `.claude/agents/` stammen aus dieser Vorlage. **Bist du berechtigt, diese Inhalte zu veröffentlichen?**
  Default: Ja, Vorlage ist deine eigene Arbeit; Nennungen im aktuellen Stand auf „agent-team-vorlage (privat)“ kürzen.
- **b) Private Pfade:** Vault-Struktur (`Tracklab/…`, `…`, `…`) und `<Vault>/…` im
  Auftrag. Default: im aktuellen Stand lassen (harmlos), **keine** Historie umschreiben (Force-Push auf `main`).
- **c) Name „Tracklab“:** Der Auftrag (§ Start, Punkt 5) verlangt vor „öffentlich“ eine Markenprüfung; bekannt ist
  2Simple „Tracklab“ (Musik-Lernsoftware). Default: Name bleibt bis v1, Prüfung vor dem ersten Release (eigene Karte).
- **d) CI nach dem Umstieg:** Öffentliche Repos haben unbegrenzte Standard-Runner-Minuten. Default: E44 (nur `main`,
  nur Code-Änderungen) bleibt; Release-Beine laufen wieder bei jedem Code-Push auf `main`.
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
