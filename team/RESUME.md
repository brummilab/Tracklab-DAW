# Resume-Board

Diese Datei liest der Team-Lead bei „weiter“ zuerst. Am Ende jedes Arbeitsschritts aktualisieren.

**Stand:** 09.10.2026 · **Meilenstein:** M1 (Fundament) · **Status:** M1-01/02/03/06, O-04/08/09/11, M1-04 erledigt – M1-04 Projektformat erledigt – **M1-05 Autosave/Backups und M1-07 CLI im Review**

> **Angehalten 08.10.2026:** Beide Implementer auf Wunsch des PO gestoppt, ungeprüft, nichts gemerged.
> Stand: `m1-03-impl` @ `99c56ca` (Umsetzung committet, Build/Format-Lauf unterbrochen),
> `m1-06-impl` @ `9aafc75` (Umsetzung + Lizenzprüfung committet, RTSan grün, tidy-Befunde offen).
> **CI (E44):** Actions-Minuten des Kontos aufgebraucht; `gate.yml` läuft nur noch bei Push auf `main` mit Code-Änderung
> (Debug + RTSan), volle Matrix manuell. Bis neue Minuten da sind, zählt nur das lokale Gate.
> **Öffentlich (E45):** Repo ist seit 09.10.2026 öffentlich (David); F43 entschieden (E46). CI-Minuten wieder verfügbar.
> **Historie umgeschrieben (E47, 09.10.2026):** alle Commit-IDs neu. Die alten Worktrees sind entfernt; `m1-03-impl`
> (`2d83353`), `m1-03-tests`, `m1-06-impl` (`4172af2`), `m1-06-tests` sind seit 09.10.2026 auf GitHub gesichert
> (PO: „ja sichern“). Sie tragen noch die alte `gate.yml` (CI bei jedem Push) – ausgelöste Läufe wurden abgebrochen;
> beim Fortsetzen zuerst `main` hineinmergen.
> **O-08 erledigt (09.10.2026):** Windows-CI war seit M1-01 rot (Link-Fehler WindowsMediaAudioFormat); Fix gemerged,
> CI auf `main` komplett grün (Run 37906889207).
> Neue Worktrees auf diese Branches anlegen. Fortsetzen nur auf Anweisung des PO: Implementer zu Ende führen → Review → Merge.

## Hinweise für neue Sessions (09.10.2026)
- Sub-Agents immer von `origin/main` abzweigen (`git branch -f main origin/main` vor jedem Auftrag).
- REA (Reverse-Engineering-MCP) installiert David optional per Setup-Script der Umgebung:
  `npx -y rea-agents@6.1.0 setup --yes --client claude_code`. Nicht für Tracklab-Code ohne Auftrag (Lizenzrisiko).
- Mockups: Artifact 8K6W6W2KNm4kxGyfjznbD8, Canva DAHXhw2g9-8; Leitbild E50 (modern, aufgeräumt, dunkel).

## PO-Wunsch: Bescheid geben, sobald testbar (09.10.2026)
David will informiert werden, sobald er selbst testen kann – im Chat **und** per Push-Benachrichtigung, mit
kopierbaren Schritten und Download-Link. Stufen:
1. **Nach M1-07 (CLI):** Handtest V1 Audio-Geräte nach `docs/testing/manual/M1.md` (Windows + Linux).
2. **Nach M1-08/M1-09:** erstes Programmfenster + Test-Builds als Download (Actions-Artefakt, Link in RESUME).
3. **Ende M2:** erster echter Arbeitsablauf A (Mitschnitt importieren, schneiden, normalisieren, exportieren).

## Zuletzt erledigt

- **M1-04** gemerged (09.10.2026): Projektformat `.tracklab` – `project.new/open/save/save_as/close/get_info`, atomares
  Speichern, Formatversion + Migrationsrahmen, relative Pfade, keine Personendaten, Namensprüfung nach Windows-Regeln.
  „Geändert“-Flag gegen Tracktions Timer abgesichert (Nachblick 650 ms mit Inhaltsvergleich). Review 2 Runden, CI grün.
  Folgekarte O-13 (`save_as` mit Medien/Undo).

- M0-01 Grundgerüst, Branding, Gate (Doku/Secrets), CI, Git-Hook (Commit `192753d`).
- M0-02 Recherche-Themen, Entscheidungsrunde 1, Design Rev 1.
- M0-03 Vorlage `agent-team-vorlage` **v1.0.0** übernommen (PO-ZIP): Prozessvertrag `team/README.md` mit
  Tracklab-Ergänzungen, Agents `researcher`, `test-writer`, `implementer`, `implementer-rt`, `reviewer`, `cleanup`
  (Modelle/Effort nach Auftrag §3.4), Board als Dateien, `plan/PLAN.md`, `CLAUDE.md` mit Vorlagen-Abschnitt.
  Hook erlaubt jetzt `git merge --ff-only` im eigenen Worktree (F14, Default A).
- **M1-02** gemerged (`6aa3807`): `src/core` Command-Registry v1 (Schema-Validierung nlohmann + pboettch Draft-7-Subset,
  Fehler mit Feld-Pointer und Klartext für Claude, Tool-Namen, `invalid_flags`/`handler_failed`/`invalid_metadata`, Export
  `tools.json`/`docs/commands.md` mit Aktualitätstest), `.gitattributes`, MSVC `/utf-8`. Review 2 Runden. Folgekarte O-06.
- **O-04** gemerged: dauerhafter App-Cache, private Caches pro Benutzer (0700), sicheres Aufräumen (lstat/uid, Lock-Prüfung),
  Logs ohne Home-Pfad, Tests nie in echten Benutzerordnern. Review 2 Runden (Sicherheitsbefund behoben). Folgekarte O-05.
- **M1-01** gemerged (`2097ea1`): Submodule unter `third_party/`, `cmake/TracklabDeps.cmake` (JUCE/Tracktion einmal
  kompiliert, Spike + `src/` teilen), `src/engine` Engine-Fabrik (Settings atomar/in-memory, headless UIBehaviour,
  `getUserName()`="Tracklab"), `tracklab_tests`. Gate all grün. Folgekarte O-04.
- **M0-07 Gate auf CMake** gemerged (`f0d26f0`): Root-CMake + Presets; `gate.sh static|build|tidy|rtsan|all`,
  `gate.ps1` (MSVC); clang-format/-tidy nur eigener Code; RTSan (Clang 20) mit 20 Fremdcode-Suppressions, Negativtest,
  `-Wfunction-effects`; ein Workflow `gate.yml`. Review 2 Runden (`team/reviews/M0-07.md`). Folgekarte O-02.
- **M0-06 Engine-Spike** gemerged (`33c5eff`): JUCE 9.0.3 + Tracktion `develop` bauen zusammen; 57/57 Tests grün
  (GCC/Clang); Bericht `team/research/engine-spike/BERICHT.md`; Review 2 Runden (`team/reviews/M0-06.md`).
  **CI grün** inkl. Windows/MSVC, MP3 auf 6 Kombinationen bitidentisch (Run 37762180378).
  Folgekarten M2-01 (MP3 gapless), M4-01 (Latenz-Blockversatz), O-01 (CI-Pakete, Upstream-Issue).
- Entscheidungsrunden 1+2: „Defaults ok“ → E1–E38; Lizenz AGPLv3 (`LICENSE`); Design Rev 2; ADR-001-Entwurf
  (`docs/adr/ADR-001-tech-stack.md`); Präfixregel E15 in `team/README.md`.
- M0-04 Recherche: 10 Themen mit `NOTIZEN.md` (Quellen, Stand 08.10.2026). Wichtigste Befunde: **JUCE 9.0.3 ist
  erschienen** (Auftrag §5 veraltet); Tracktion `develop` = 3.5.0 ungetaggt mit LUFS/Render-Queue/Mehrkanal; MCP-Spec
  2026-07-28 (zustandslos), kein C++-SDK; Claude-API max. 20 strict-Tools, `tool_choice any/tool` auf Opus/Sonnet 5.5 verboten;
  AGPLv3 bestätigt, Namenstreffer 2Simple „Tracklab“; JUCE-ALSA ohne RT-Thread → JACK über PipeWire; kein CLAP in JUCE 9;
  Screenshots laufen ohne Display. Brief-Vorschläge in M0-06 und M0-07. Entscheidungsrunde 2 (F16–F38).

## Wartet auf den PO

- Infos ohne Default: F9 Audio-Interface, F10 Reaper-MCP, F11 `reaper-kb.ini`, F12 lokal/Cloud, F22 Test-API-Key,
  F28b EBU-Quellen (optional).

## Nächster Schritt (Lead)

1. M1 läuft nach Abhängigkeiten (max. 4 parallel): **M1-01** zuerst → dann M1-02 und M1-06 parallel → M1-03 → M1-04 →
   M1-05 und M1-07 → M1-08 (nach M1-02/M1-04) → M1-09. Je Karte: test-writer → implementer → reviewer → Gate → Merge → CI.
2. M0-08 (Reaper-Preset) ruht bis F11; Umsetzung M9. O-02 vor Karten mit eigenen Graph-Nodes; O-03 vor Mint-23-Support.

## Offene Karten

| Spalte | Karten |
|---|---|
| in Arbeit | – |
| Review | M1-05 · M1-07 (Reviewer + CI) |
| Backlog | O-13 · O-12 (CI-Flakes) · O-10 · M1-08 · M1-09 · O-05 · O-06 · O-07 · M0-08 Rest-Recherche (daw-features) · M2-01 · M4-01 · O-01 · O-02 · O-03 |
| Erledigt | M0-01 · M0-02 · M0-03 · M0-04 · M0-05 · M0-06 · M0-07 · M1-01 · M1-02 · M1-03 · M1-04 · M1-06 · O-04 · O-08 · O-09 · O-11 |

## Builds

Spike-Artefakte (`spike_cli`, `SpikeGain.vst3`) ab dem ersten Lauf von Workflow `spike-engine` auf `main` (GitHub Actions).
