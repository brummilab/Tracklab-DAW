<p align="center">
  <img src="assets/branding/png/tracklab-logo-light.png" alt="Tracklab" width="480">
</p>

# Tracklab

Eigene Digital Audio Workstation für **Windows 10/11** und **Linux (Linux Mint / Ubuntu)** –
für Live-Mitschnitte, Band-Recording, Editing, Mixing und Mastering. Claude ist integriert und
führt Befehle direkt in Tracklab aus; zusätzlich lässt sich Tracklab über einen lokalen
MCP-Server von Claude Desktop und Claude Code steuern. Tracklab bleibt **ohne Claude voll nutzbar**.

## Ziel

- Für Reaper-Nutzer sofort vertraut: Actions, Shortcuts, Routing-Freiheit, Regionen, Render-Matrix.
- Erster Anwendungsfall (Workflow A): Konzertmitschnitt importieren, per Setlist in Songs schneiden,
  Fades setzen, auf −14 LUFS / −1 dBTP normalisieren und pro Song als WAV 48 kHz/24-bit (für
  DaVinci Resolve) und MP3 exportieren.
- Danach (Workflow B): Multitrack-Bandaufnahme mit 12+ Eingängen, Comping, Mix und Mastering.
- Jede Aktion gleichwertig per Maus, Tastatur, Controller, Claude-Panel und MCP-Client.

Details: [`docs/auftrag/Claude-Code-Prompt.md`](docs/auftrag/Claude-Code-Prompt.md) und
[`team/design/DESIGN.md`](team/design/DESIGN.md).

## Status

**Meilenstein M0 – Einrichtung.** Es gibt noch keinen lauffähigen Code.

- Grundgerüst, Branding, Gate (Doku-, Agent- und Secret-Checks) und CI stehen.
- Agent-Team-Vorlage v1.0.0 ist übernommen: Prozessvertrag `team/README.md`, Sub-Agents in `.claude/agents/`
  (researcher, test-writer, implementer, implementer-rt, reviewer, cleanup), Board als Dateien unter `team/board/`.
- Recherche abgeschlossen (`team/research/*/NOTIZEN.md`), Entscheidungsrunden 1 und 2 entschieden
  (`team/ENTSCHEIDUNGEN.md`), Design Rev 2.
- Tech-Stack (ADR-001, Entwurf: [`docs/adr/ADR-001-tech-stack.md`](docs/adr/ADR-001-tech-stack.md)): C++20/CMake,
  JUCE 9, Tracktion Engine 3.5 – unter Vorbehalt des Engine-Spikes.
- **Jetzt:** Engine-Spike (JUCE + Tracktion Engine) mit CI-Build für Windows und Linux, danach Gate auf CMake.

Aktueller Arbeitsstand: [`team/RESUME.md`](team/RESUME.md).

## Builds

Noch keine. Sobald der Engine-Spike steht, erzeugt jeder Push auf `main` Test-Builds als Download
in den GitHub-Actions-Läufen (Workflow `gate`). Den jeweils aktuellen Link trägt der Team Lead in
`team/RESUME.md` ein.

## Repo-Struktur

| Pfad | Inhalt |
|---|---|
| `assets/branding/` | Logo, App-Icon (SVG, PNG, ICO), Farb-Tokens |
| `src/`, `tests/` | Code und Tests (ab M1) |
| `docs/` | Auftrag, Echtzeit-Regeln, Command-Referenz, Testprotokolle |
| `team/` | Prozess (Agent-Team-Vorlage): Resume, To-do für den PO, Entscheidungen, Design, Plan, Board, Recherche, Reviews |
| `.claude/` | Agent-Definitionen und Hook-Einstellungen für Claude Code |
| `scripts/` | Gate (`gate.sh`/`gate.ps1`), Claude-Code-Hooks |
| `.github/workflows/` | CI (`gate.yml`, Matrix Windows + Linux) |

## Mitarbeit

Entwicklung mit Claude Code als Team Lead und Sub-Agents nach der Agent-Team-Vorlage v1.0.0; Regeln in
[`CLAUDE.md`](CLAUDE.md), Prozessvertrag in [`team/README.md`](team/README.md).
Gate lokal: `bash scripts/gate.sh` (Linux) bzw. `pwsh scripts/gate.ps1` (Windows).

## Lizenz

GNU Affero General Public License v3.0 (`AGPL-3.0-only`, siehe [`LICENSE`](LICENSE)) – Folge aus JUCE (AGPLv3) und
Tracktion Engine (GPLv3). Wer einen Build erhält, bekommt auch den Quelltext. Repository vorerst privat.
