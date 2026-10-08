# CLAUDE.md – Tracklab

Tracklab ist eine eigene DAW für Windows 10/11 und Linux (Mint/Ubuntu) mit integriertem Claude
und lokalem MCP-Server. Vollständiger Auftrag: `docs/auftrag/Claude-Code-Prompt.md`.
Design: `team/design/DESIGN.md`. Aktueller Stand und nächster Schritt: `team/RESUME.md`.

## Rollen
- **Product Owner:** David – Scope, Entscheidungen, Builds prüfen, Releases freigeben, alles mit sudo.
- **Team Lead:** Hauptsession – Recherche, Design, Plan, Briefs, Gate, Merge, Commit, Push, Aufzeichnungen.
  Schreibt keinen Produktionscode (außer trivialen Merge-Konflikt-Fixes), bewertet keine eigene Arbeit.
- **Team Members:** Sub-Agents (`.claude/agents/`) – ein Paket je Brief, eigener Worktree, nie `main`.
- Höchstens **4 Sub-Agents gleichzeitig** (WIP-Limit 4 auf `team/BOARD.md`).

## Vorlage
<!-- PLATZHALTER: Hier wird CLAUDE-abschnitt.md aus agent-team-vorlage eingesetzt
     (§3.1 Schritt 2). Die Vorlage war in Session 1 nicht erreichbar – siehe team/TODO-PO.md F0. -->
Bis zur Übernahme gilt der Loop aus `docs/auftrag/Claude-Code-Prompt.md` §3.2.

## Arbeitsregeln (Kurzfassung, Details §2 im Auftrag)
- **R1** `Projektinhalt.md` bei jedem neuen Eintrag aktualisieren; Inhalte in den Vault
  (`Tracklab/Projektinhalt.md`) übernehmen.
- **R2** Vault ist Single Source of Truth: vor jeder Aufgabe relevante Notizen lesen (Obsidian-MCP).
- **R3/R4** Kein PR-Workflow. Nur der Team Lead merged nach grünem Gate lokal in `main` und pusht direkt.
  Bei jedem Merge: README, `Projektinhalt.md`, Vault-Abschnitte *Status*, *Offene Punkte*,
  *Entscheidungen* und `team/RESUME.md` nachziehen.
- **R5** Im Vault nur *Status*, *Offene Punkte*, *Entscheidungen* abschnittsweise patchen; `## Verlauf`
  schreibt der Git-Hook.
- **R6** Niemals ein repo-lokales `core.hooksPath` setzen (globale Hooks `~/.githooks` müssen laufen).
- **R7** Obsidian-MCP: `vault_get_document_map` holen, Überschriften als Pfad-Array
  (z. B. `["Tracklab", "Status"]`), `version` als `ifMatch`; `vault_move` nur Einzeldateien.
- **R8** Doku/README/Entscheidungen Deutsch; Code, Bezeichner, Kommentare Englisch.
  Commits Deutsch: `<Karte>: <Text>`, Prozess `team: …`, Tests `Tests: …`.
- **R9** sudo/Systeminstallationen macht David → `USER INPUT REQUIRED` mit kopierbaren Befehlen.
- **R10** Knapp, Deutsch, komplette Dateien; jede GUI-Änderung mit Screenshot (`docs/screenshots/<karte>/`).
- **R12** Nichts erfinden. Unsicheres als `[VERIFIZIEREN]` markieren und in `team/research/` klären.
- **R13** Keine Secrets, keine echten Band-Mitschnitte im Repo.

## Echtzeit-Regeln (MUSS, Details `docs/realtime.md`)
Im Audio-Thread: keine Allokation, keine Locks, kein IO, kein Logging außer Lock-free-Ringpuffer,
keine Exceptions, keine blockierenden Systemaufrufe. Kommunikation über Lock-free-Queues/atomare
Snapshots. Audio-Callbacks `[[clang::nonblocking]]`, RealtimeSanitizer im Gate.

## Claude-Sicherheit (MUSS, Details Auftrag §7.2)
- Jeder Claude-Turn = eine Undo-Transaktion („Claude: <Kurzbeschreibung>“).
- `destructive`-Commands brauchen in jedem Modus eine Bestätigung.
- Max. 50 Commands pro Turn (einstellbar), Timeouts, Abbruch jederzeit.
- Keine Shell, kein beliebiger Dateizugriff – nur Projektordner und freigegebene Ordner.
- Jede Aktion ist ein Command der Registry (JSON-Schema mit `additionalProperties: false`).
  GUI, Shortcuts, Claude-Panel, MCP und CLI nutzen dieselbe Registry – kein zweiter Codepfad.

## Gate
`scripts/gate.sh` (Linux) / `scripts/gate.ps1` (Windows); CI: `.github/workflows/gate.yml`.
Handoff an den Team Lead erst, wenn das Gate lokal grün ist. DoD: `team/design/DESIGN.md` → Gate.

## Hooks
`.claude/settings.json`: `PreToolUse` auf Bash → `scripts/hooks/guard-git.sh` blockiert in
Worktree-Sessions `git push`, `git merge`, `git checkout main` und Commits auf `main`.

## USER INPUT REQUIRED
1. Eintrag in `team/TODO-PO.md`. 2. `team/RESUME.md` auf `BLOCKED: USER INPUT REQUIRED` setzen,
committen, pushen. 3. Im Chat ausgeben und stoppen:
```
=== USER INPUT REQUIRED ===
Was: <eine Zeile>
Warum: <eine Zeile>
Aktion für David: <nummerierte, kopierbare Schritte/Befehle>
Antwort bitte: hier im Chat oder in team/TODO-PO.md
Ohne Antwort mache ich weiter mit: <nichts | Default X>
```

## „weiter“
Neue Session: `team/RESUME.md` lesen und dort weitermachen. Nichts darf nur im Chat stehen.
