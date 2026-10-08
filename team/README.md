# Agent-Team: Prozessvertrag

Vorlage `agent-team-vorlage` v1.0.0 (übernommen am 08.10.2026) · gilt für jede Claude-Code-Sitzung in diesem Repo.
Tracklab-Ergänzungen stehen gesammelt am Ende („Tracklab-Ergänzungen“).

**Grundsatz:** Der **Product Owner** (PO, ein Mensch) setzt Themen und gibt frei. Die Hauptsitzung ist der
**Team-Lead** (Orchestrator). Die Arbeit erledigen **Sub-Agents** (`.claude/agents/`). Der gesamte Zustand liegt in
Dateien unter `team/`, nicht im Chatverlauf: Eine neue Sitzung muss nach „weiter“ ohne Rückfragen dort weitermachen
können, wo die letzte aufgehört hat.

## Rollen

| Rolle | Wer | Macht | Macht nie |
|---|---|---|---|
| **Product Owner** | ein Mensch | Setzt Themen und Umfang, beantwortet Entscheidungsrunden, prüft Builds, gibt Releases frei, führt alles aus, was sudo/Server-Zugang/Zugangsdaten braucht | Design, Code |
| **Team-Lead** | Hauptsitzung (starkes Modell, hohe Effort-Stufe) | Recherche anstoßen, Design, Plan, Briefs schreiben, Lücken entscheiden, Gate prüfen, mergen, committen, alles protokollieren | Eigene Arbeit bewerten (dafür gibt es den `reviewer`) |
| **Team-Mitglieder** | Sub-Agents mit festem Modell und Effort | Je ein Brief, eigener Git-Worktree | `main` anfassen, mehr als ihre zugewiesenen Dateien ändern |

Team-Mitglieder (Definitionen in `.claude/agents/`):

| Agent | Modell · Effort | Worktree | Aufgabe |
|---|---|---|---|
| `researcher` | sonnet-5-5 · medium | nein (nur lesen) | Fragen klären, Code/Doku durchsuchen, Ergebnis für `team/research/` |
| `test-writer` | sonnet-5-5 · high | ja | Tests zuerst: schreibt die Tests aus dem Brief, die vor der Umsetzung rot sind; meldet Lücken im Design |
| `implementer` | sonnet-5-5 · high | ja | Setzt den Brief um, bis die Tests grün sind, ohne Tests zu ändern |
| `implementer-rt` | opus-5-5 · high | ja | Wie `implementer`, für Audio-Thread, DSP und Plugin-Sandbox (Tracklab) |
| `reviewer` | opus-5-5 · high | nein (nur lesen + Gate) | Prüft das Ergebnis gegen Design und Brief, Urteil: OK / Nacharbeit |
| `cleanup` | haiku-5-5 · low | ja | Aufräumen ohne Verhaltensänderung (Formatierung, tote Includes, Doku-Links) |

Höchstens **4 Sub-Agents gleichzeitig**. Parallel nur bei Briefs mit disjunkten Dateien.

## Das Gedächtnis: `team/`

| Datei/Ordner | Inhalt |
|---|---|
| `RESUME.md` | Wo stehen wir, was ist der nächste Schritt. Wird **am Ende jedes Arbeitsschritts** aktualisiert. |
| `TODO-PO.md` | **Nur** offene Entscheidungen und Reviews für den PO. Nichts anderes. |
| `ENTSCHEIDUNGEN.md` | Protokoll aller beantworteten Entscheidungen (Datum, Frage, Antwort, Folge), neueste oben. |
| `design/DESIGN.md` | Die Spezifikation, mit nummerierten Revisionen und Änderungslog. |
| `research/<thema>/` | Ein Ordner pro Thema: Fragen, Befunde, Quellen. |
| `plan/PLAN.md` | Meilensteine mit Zielen und Abnahmekriterien. |
| `board/` | Kanban als Dateien: ein Brief pro Karte, Spalte = Ordner (`backlog/`, `in-arbeit/`, `review/`, `erledigt/`). Karte wandert per `git mv`. |
| `reviews/` | Ein Review-Protokoll pro Karte (`<karte>.md`), geschrieben vom `reviewer`. |
| `RUECKBLICK-<M>.md` | Rückblick nach jedem Meilenstein: Zahlen, was lief gut, was hakte, Prozessanpassungen. |

## Die Schleife (jedes Thema, jeder Meilenstein)

1. **PO wählt ein Thema** → Lead legt `research/<thema>/` an.
2. **Recherche + Fragen** → `researcher` klärt, was ohne PO geht; offene Fragen landen als Entscheidungsrunde in
   `TODO-PO.md`. Kennzeichnung im Chat: **„USER INPUT REQUIRED“**, jeweils mit Optionen und Empfehlung.
3. **PO entscheidet** → Lead überträgt die Antworten nach `ENTSCHEIDUNGEN.md`.
4. **Design-Revision** → Lead schreibt die nächste Revision in `design/DESIGN.md`, plant in `plan/PLAN.md` und
   schreibt Briefs nach `board/backlog/`.
5. **Tests zuerst, dann Code** → je Brief: `test-writer` (Tests rot), dann `implementer` (Tests grün), beide im
   eigenen Worktree. Meldet der Test-Writer „Unklarheiten“ (Lücken im Design), entscheidet der Lead sie **vor** dem
   Implementer und hält sie im Design oder Brief fest.
6. **Review gegen das Design** → `reviewer` prüft den Branch gegen Brief und Design; der Lead speichert den Bericht
   als `reviews/<karte>.md`. Bei „Nacharbeit“ zurück zu Schritt 5. Hinweise aus dem Review werden Folgekarten.
7. **Gate, Merge** → Lead führt `./scripts/gate.sh` aus, merged den Branch in `main`, führt das Gate auf `main` erneut
   aus, pusht. CI führt das Gate noch einmal aus (Push auf `main` prüft nur, rollt nichts aus).
8. **PO prüft → Release** → Freigabe im Chat; danach Release-Tag (siehe Regeln). PO-Anmerkungen werden zur nächsten
   Runde (zurück zu 1).

Nach jedem Meilenstein ein **Rückblick**; alle paar Meilensteine eine **Optimierungsrunde** (`cleanup` + Prozess
anpassen). Bei einem neuen Modell eine **Vergleichsrunde** (gleicher Brief, altes vs. neues Modell).

## Briefs

Jeder Brief folgt `board/BRIEF-VORLAGE.md` und enthält genau drei Dinge: **Fakten** (was der Agent wissen muss),
**eigene Dateien** (was er ändern darf, sonst nichts), **Bewertung** (woran der `reviewer` „fertig“ festmacht). Ein
Sub-Agent bekommt nur den Brief, nicht den Chatverlauf. Lead-Entscheidungen nach Rückfragen kommen als eigener
Abschnitt in den Brief.

## Regeln

- **Gate = `./scripts/gate.sh` grün.** Ohne grünes Gate kein Merge, kein Push.
- Der Lead **bewertet nie seine eigene Arbeit**: Jede Code-Änderung geht durch den `reviewer`. Ausnahme: reine Doku-
  und `team/`-Änderungen.
- Der Lead liest jeden Implementer-Bericht auf „Abweichungen vom Brief“ und Zusatzentscheidungen und gibt daraus
  **konkrete Prüfpunkte** an den Reviewer. (Das hat in der Praxis die entscheidenden Funde gebracht.)
- Code-ändernde Sub-Agents (`test-writer`, `implementer`, `cleanup`) arbeiten nur in ihrem Worktree und committen dort,
  sie pushen nie. `researcher` und `reviewer` ändern nichts. Nur der Lead merged nach `main` und pusht.
- Der Implementer holt die Tests per `git merge --ff-only <test-branch>` (ein Branch kann nicht in zwei Worktrees
  ausgecheckt sein); geht das nicht, normaler Merge, im Bericht nennen. Der Reviewer prüft in
  `git worktree add --detach /tmp/review-<karte> <branch>`.
- **Live geht ein Stand nur per Release** (z. B. Tag `vJJJJ.MM.n`) und nur nach **ausdrücklicher Freigabe des PO in der
  laufenden Unterhaltung**. Nach dem Release prüft der Lead per API, ob Release/Tag auf dem erwarteten Commit liegen,
  und dann den Lauf. Kann der Lead keine Tags pushen (z. B. Git-Proxy in Cloud-Sitzungen), legt der PO das Release an;
  nicht umgehen.
- **Datenschutz:** In Tests, Code und Kommentaren nur erfundene Fake-Daten. Nie echte Namen/IDs aus Git-Historie,
  Commit-Nachrichten oder Doku übernehmen.
- Alles, was sudo, Server-Zugang, Zugangsdaten oder eine fachliche Entscheidung braucht, geht als Punkt nach
  `TODO-PO.md`, nie raten. Geheimnisse nie in Git oder Chat.
- Neue Agent-Definitionen in `.claude/agents/` lädt Claude Code erst beim nächsten Sitzungsstart. In der Sitzung, die
  sie anlegt, startet der Lead sie als `general-purpose` mit der Definition als Auftrag.
- Board-Commits bündeln, solange Agents in Worktrees arbeiten (weniger Merge-Rauschen).
- Nach einem Container- oder Sitzungsneustart: `git worktree list` und Branches prüfen, halbe Review-Worktrees unter
  `/tmp` entfernen, verlorene Agents neu starten (Branches unter `.claude/worktrees/` bleiben erhalten).
- `RESUME.md` ist vor jedem Sitzungsende aktuell.

## „weiter“ in einer neuen Sitzung

Lead liest in dieser Reihenfolge: `team/RESUME.md` → `team/TODO-PO.md` (neu beantwortet?) → die Karten in
`board/in-arbeit/` und `board/review/` → macht beim nächsten Schritt weiter.

## Tracklab-Ergänzungen

Bei Prozessfragen gilt die Vorlage, bei Produkt- und Technikfragen der Auftrag
(`docs/auftrag/Claude-Code-Prompt.md`). Widersprüche entscheidet der PO (`TODO-PO.md`).

- **PO:** David. **Lead:** Hauptsitzung mit `claude-opus-5-5`, Effort hoch.
- **Nummern:** Meilenstein-Karten `M<n>-<nr>`, Optimierungskarten `O-<nr>`, Entscheidungen `E<nr>`.
- **Board-Dateien:** `board/<spalte>/<karte>.md` nach `board/BRIEF-VORLAGE.md`; WIP-Limit 4 in `in-arbeit/`.
- **Modelle:** Agent-Definitionen pinnen volle Modell-IDs (`claude-sonnet-5-5`, `claude-opus-5-5`,
  `claude-haiku-5-5`) `[VERIFIZIEREN per /model]`. Karten für Audio-Thread, DSP oder Plugin-Sandbox gehen an
  `implementer-rt` statt `implementer`.
- **Sprache:** Doku, Briefs, Reviews Deutsch; Code, Bezeichner und Code-Kommentare **Englisch** (Auftrag R8,
  weicht vom Vorlagen-Implementer ab). Test-Namen ebenfalls Englisch.
- **Gate:** `scripts/gate.sh` (Linux) bzw. `scripts/gate.ps1` (Windows); CI-Matrix Windows + Linux bei jedem Push.
  DoD je Karte: `team/design/DESIGN.md` → „Gate“.
- **Push:** Kein PR-Workflow; nur der Lead merged nach grünem Gate lokal in `main` und pusht direkt (Auftrag R3/R4).
  Bei jedem Merge: README, `Projektinhalt.md`, Vault-Abschnitte *Status*/*Offene Punkte*/*Entscheidungen*,
  `RESUME.md` nachziehen.
- **Hook:** `scripts/hooks/guard-git.sh` blockiert in Worktree-Sitzungen `git push`, `git checkout main`, Commits
  auf `main` und `git merge`, solange `HEAD` auf `main` steht. `git merge --ff-only <test-branch>` im eigenen
  Worktree bleibt erlaubt (Implementer-Schritt der Vorlage, siehe `TODO-PO.md`).
- **Testing ohne Hardware:** CI prüft Build, Unit-, Golden- und Headless-Tests. Audio-Hardware-Tests laufen bei
  David nach `docs/testing/manual/<meilenstein>.md` als Prüfaufgabe in `TODO-PO.md`.
- **Agent Teams (experimentell):** nicht verwenden, außer der PO entscheidet es.
