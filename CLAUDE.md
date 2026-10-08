# CLAUDE.md – Tracklab

Tracklab ist eine eigene DAW für Windows 10/11 und Linux (Mint/Ubuntu) mit integriertem Claude
und lokalem MCP-Server. Vollständiger Auftrag: `docs/auftrag/Claude-Code-Prompt.md`.
Design: `team/design/DESIGN.md`. Aktueller Stand und nächster Schritt: `team/RESUME.md`.

## Rollen
- **Product Owner:** David – Scope, Entscheidungen, Builds prüfen, Releases freigeben, alles mit sudo.
- **Team Lead:** Hauptsession – Recherche, Design, Plan, Briefs, Gate, Merge, Commit, Push, Aufzeichnungen.
  Schreibt keinen Produktionscode (außer trivialen Merge-Konflikt-Fixes), bewertet keine eigene Arbeit.
- **Team Members:** Sub-Agents (`.claude/agents/`) – ein Paket je Brief, eigener Worktree, nie `main`.
- Höchstens **4 Sub-Agents gleichzeitig** (WIP-Limit 4 in `team/board/in-arbeit/`).

## Agent Team Process
<!-- From agent-team-vorlage v1.0.0 (CLAUDE-abschnitt.md), adopted 08.10.2026. -->

This repo is worked on by an agent team. **Read `team/README.md` (the process contract) before any task.** Short version:

- David is the product owner. The main session is the **team lead** (orchestrator): research, design, plan, briefs,
  gate, merge, commit, keep every record. It never grades its own work: every code change goes through the `reviewer`
  sub-agent.
- Work is done by sub-agents in `.claude/agents/` (`researcher`, `test-writer`, `implementer`, `implementer-rt`,
  `reviewer`, `cleanup`), each with a pinned model and effort. `test-writer`, `implementer(-rt)` and `cleanup` run in
  their own git worktree and never touch `main`. Max 4 at once.
- All state lives in `team/`: `RESUME.md`, `TODO-PO.md` (decisions and reviews only), `ENTSCHEIDUNGEN.md`,
  `design/DESIGN.md` (numbered revisions), `research/<topic>/`, `plan/PLAN.md`, `board/` (one brief per card), `reviews/`.
- When the PO says **"weiter"**: read `team/RESUME.md`, then `team/TODO-PO.md`, then the cards in `board/in-arbeit/` and
  `board/review/`, and continue. Update `team/RESUME.md` before ending.
- Anything needing a decision, sudo, server access or credentials goes to `team/TODO-PO.md`, flagged in chat as
  **USER INPUT REQUIRED**. Never guess.
- **Gate:** `./scripts/gate.sh` must be green before merging to `main`.
- **Release:** only after the PO explicitly releases in the current conversation.

Bei Prozessfragen gilt die Vorlage, bei Produkt- und Technikfragen der Auftrag. Tracklab-Ergänzungen zum Prozess:
`team/README.md` → „Tracklab-Ergänzungen“.

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
Worktree-Sessions `git push`, `git checkout main` sowie Commit/Merge, solange `HEAD` auf `main` steht.

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
