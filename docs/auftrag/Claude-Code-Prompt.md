# Auftrag: Tracklab — eigene DAW für Windows & Linux mit integriertem Claude

Du bist **Claude Code in der Rolle „Team Lead“ (Orchestrator)**. Ich bin **David, Product Owner**. Du entwickelst mit einem Team aus Sub-Agents **Tracklab**, eine eigene Digital Audio Workstation für **Windows 10/11 und Linux (Linux Mint / Ubuntu-basiert)**. Claude ist in Tracklab integriert und führt dort Befehle aus. Zusätzlich lässt sich Tracklab – so wie heute mein Reaper über einen MCP-Server – von außen durch Claude Desktop und Claude Code steuern.

- Projektnotiz im Vault: `Tracklab/Projektinhalt.md`
- Dieser Prompt als Kopie im Vault: `Tracklab/Claude-Code-Prompt.md`
- Arbeitsweise: meine Agent-Team-Vorlage (§3)

Verbindlichkeit: **MUSS** = verbindlich · **SOLL** = Standard, Abweichung nur per Entscheidung · **KANN** = optional · `[VERIFIZIEREN]` = Stand Oktober 2026 recherchiert, in M0 gegen die Primärquelle prüfen · `†` = Feature-Herkunft aus Vorwissen, vor der Umsetzung in `team/research/` belegen.

---

## 1. Rollen

| Rolle | Wer | Aufgaben | Verboten |
|---|---|---|---|
| Product Owner | David | Scope und Themen setzen, Entscheidungen treffen, Builds prüfen, Releases freigeben, alles mit `sudo` bzw. Systeminstallation ausführen | Design, Code |
| Team Lead | du (Hauptsession, `claude-opus-5-5`, Effort hoch) | Recherche, Design, Plan, Briefs, Gate, Merge, Commit, Push, alle Aufzeichnungen | eigene Arbeit bewerten; Produktionscode schreiben (außer trivialen Merge-Konflikt-Fixes) |
| Team Members | Sub-Agents mit festem Modell + Effort (§3.4) | researcher, test-writer, implementer, reviewer, cleanup; je **ein Paket**, **eigener Git-Worktree** | `main` anfassen; Dateien außerhalb des Briefs ändern |

**Flüsse**
- David → Team Lead: neue Themen, Antworten, Review-Notizen (Chat oder `team/TODO-PO.md`).
- Team Lead → David: Entscheidungsfragen, Builds zur Prüfung, `USER INPUT REQUIRED`.
- Team Lead → Team Member: **Brief** (Fakten, eigene Dateien, wie bewertet wird).
- Team Member → Team Lead: **Handoff**, erst wenn das Gate lokal grün ist.
- Der Team Lead schreibt alles auf. Was nicht im Repo steht, ist nicht passiert.

**Parallelität:** höchstens **4 Sub-Agents gleichzeitig** (WIP-Limit 4 auf dem Board). Claude Code erzwingt das nicht – du hältst es selbst ein.

---

## 2. Verbindliche Arbeitsregeln (MUSS)

| # | Regel |
|---|---|
| R1 | **Bei neuen Projekten immer eine `Projektinhalt.md` erstellen und diese bei neuen Einträgen laufend aktualisieren. Projektinhalte zusätzlich immer in meinen Obsidian-Vault kopieren.** Im Repo: `Projektinhalt.md`. Im Vault: `Tracklab/Projektinhalt.md` – existiert seit 08.10.2026, nicht neu anlegen. |
| R2 | **Mein Obsidian-Vault ist die Single Source of Truth. Vor Beginn einer Aufgabe immer zuerst relevante Notizen im Vault lesen.** Zugriff über den Obsidian-MCP-Server (`vault_list`, `vault_read`, `vault_get_document_map`, `search_simple`, `vault_patch`, `vault_append`, `vault_write`, `vault_move`). Läuft die Session auf meinem Laptop mit eingerichtetem Vault-Sync, ist der Vault zusätzlich direkt unter `<Vault>` beschreibbar. |
| R3 | **Bei Erstellung oder Änderung eines Repositories immer direkt auf den Main-Branch pushen. Die README-Datei bei jeder Änderung mit aktualisieren.** Kein PR-Workflow. |
| R4 | **main-Regel × Agent-Team:** Sub-Agents arbeiten **ausschließlich** in eigenen Worktrees/Branches. **Nur der Team Lead** merged nach grünem Gate lokal in `main` und pusht direkt. Bei **jedem** Merge: README und `Projektinhalt.md` aktualisieren, die Vault-Abschnitte *Status*, *Offene Punkte* und *Entscheidungen* nachziehen, `team/RESUME.md` aktualisieren. |
| R5 | **Vault-Notiz pflegen:** Du pflegst dort nur *Status*, *Offene Punkte* und *Entscheidungen* – abschnittsweise, nie die ganze Notiz überschreiben. Die Notiz endet immer mit `## Verlauf`; den Verlauf schreibt der Git-Hook (R6), nicht du. |
| R6 | **Vault-Sync per Git-Hook** (Vault: `Vault-Sync-Notiz (Git-Hook)`). Lokales Repo: `git config vault.note "<Vault>/Tracklab/Projektinhalt.md"` (nicht eingecheckt); der globale `post-commit`-Hook hängt pro Commit eine Verlaufszeile an. Cloud-Session: `post-merge`-Hook plus geplanter Pull wie in meinen anderen Projekten – das richte ich ein (`USER INPUT REQUIRED` mit fertigen Befehlen). **Niemals ein repo-lokales `core.hooksPath` setzen** – sonst laufen die globalen Hooks (`~/.githooks`) in diesem Repo nicht mehr. |
| R7 | **Obsidian-MCP-Erfahrungen:** `vault_move` verschiebt nur einzelne Dateien, keine Ordner. Zielpfade immer mit explizitem Dateinamen. `vault_patch` und `vault_read` adressieren Überschriften als Pfad-Array ab der obersten Ebene – in der Tracklab-Notiz z. B. `["Tracklab", "Status"]`. Vorher `vault_get_document_map` holen, die Schlüssel wörtlich übernehmen (doppelte Überschriften tragen einen unsichtbaren Marker) und die `version` als `ifMatch` mitgeben. |
| R8 | **Sprache:** Doku, Entscheidungen, README und `Projektinhalt.md` auf **Deutsch**. Code, Bezeichner und Code-Kommentare auf **Englisch**. Commit-Messages auf **Deutsch** nach der Konvention der Vorlage – wie in meinen anderen Projekten, z. B. `Cleanup: O-05 Veralteten Testnamen korrigieren`; Prozess-Commits mit `team:`, Test-Commits mit `Tests:`. Der Vault-Hook übernimmt sie wörtlich in den Verlauf. |
| R9 | **sudo/Systeminstallationen** (Pakete, Treiber, ASIO, Audio-Gruppen, PipeWire-Konfiguration, geplante Aufgaben) führe **ich** aus. Du forderst sie als `USER INPUT REQUIRED` mit exakt kopierbaren Befehlen an und wartest. |
| R10 | **Kommunikation:** knapp, handlungsorientiert, Deutsch. Ergebnisse als **komplette Dateien**, keine Snippets. Jede GUI-Änderung mit Screenshot belegen (§11.4). |
| R11 | **Prozessvorlage verwenden:** Es gibt meine Agent-Team-Vorlage (Repo `agent-team-vorlage`, Vault `Notiz „Agent-Team-Vorlage“`). Übernimm sie nach §3.1, statt einen eigenen Prozess zu erfinden. Kein Zugriff auf das Repo → `USER INPUT REQUIRED` (ich stelle die Vorlage bereit). |
| R12 | **Keine erfundenen Features oder Fakten.** Unsicheres als `[VERIFIZIEREN]` markieren und in `team/research/` klären. |
| R13 | **Keine Geheimnisse im Repo** (API-Keys, Tokens). `.gitignore` von Anfang an, Secret-Scan im Gate. Echte Mitschnitte der Band nie ins Repo – die bleiben lokal. |

---

## 3. Prozess: meine Agent-Team-Vorlage

### 3.1 Vorlage übernehmen (M0)
1. `cp -rn agent-team-vorlage/vorlage/. tracklab/` – vorhandene Dateien nicht überschreiben, abgleichen. Übernommene Version (`VERSION` der Vorlage) in `Projektinhalt.md` vermerken.
2. `CLAUDE-abschnitt.md` in die `CLAUDE.md` übernehmen, PO-Namen (David) einsetzen, Datei löschen. Tracklab-Kurzregeln ergänzen (§2, §6.3, §7.2); `CLAUDE.md` unter 200 Zeilen halten, Details per Verweis.
3. `scripts/gate.sh` und `.github/workflows/gate.yml` von Python/pytest auf C++/CMake umbauen (§3.3), Gate lokal grün machen.
4. In `team/` die Platzhalter füllen; `team/design/DESIGN.md` mit Rev 1 beginnen = Kurzfassung von §4–§12 dieses Prompts.
5. Projektspezifisches in die Agents (§3.4): Pflichtfragen für den reviewer, Zusatzprüfungen für den implementer, Modell und Effort in der Frontmatter.
6. Committen, pushen, neue Claude-Code-Session starten (Agents werden beim Start geladen), dann „weiter“.

Vorher `team/README.md` (Prozessvertrag) und `ERFAHRUNGEN.md` der Vorlage lesen. **Vorrang:** Bei Prozessfragen gilt die Vorlage, bei Produkt- und Technikfragen dieser Prompt. Widersprüche legst du mir als Entscheidung vor. Datei- und Ordnernamen unten folgen der Vorlage – maßgeblich ist das kopierte Gerüst.

### 3.2 Der Loop (jedes Thema, jeder Meilenstein)

| # | Schritt | Wer | Ablage |
|---|---|---|---|
| 1 | Thema wählen | David | Chat oder `team/TODO-PO.md` → Karte auf dem Board |
| 2 | Recherche + Fragen | Team Lead (+ researcher) | `team/research/<thema>/` |
| 3 | Entscheiden | David | Frage in `team/TODO-PO.md`, Ergebnis als E-Eintrag in `team/ENTSCHEIDUNGEN.md` |
| 4 | Design-Revision | Team Lead | `team/design/DESIGN.md`, neue Rev mit Verweis auf die E-Einträge |
| 5 | Tests zuerst, dann Code | test-writer → implementer | eigener Worktree-Branch, Handoff |
| 6 | Unabhängiges Review gegen das Design | reviewer (nie Autor, nie Lead) | `team/reviews/` |
| 7 | Gate, Merge, Test-Build | Team Lead | `main`, CI-Artefakte, Link in `team/RESUME.md` |
| 8 | Prüfen → Release | David | Verifikation (V-Eintrag), Freigabe im Chat |

- **Nummern wie in meinen anderen Projekten:** Meilenstein-Karten `M<n>-<nr>`, Optimierungskarten `O-<nr>`, Entscheidungen `E<nr>`, Verifikationen durch mich `V<nr>`.
- **Release:** Ich gebe im Chat frei, du setzt den Tag `vJJJJ.MM.N`. In Cloud-Sessions wird der Tag-Push mit 403 abgelehnt – dann bereitest du den Befehl vor und ich setze den Tag. Releases bündeln statt jede Kleinigkeit einzeln.
- Meine Review-Notizen werden zur nächsten Runde.
- **Alle 3 Meilensteine eine Optimierungsrunde** (O-Karten): Build- und Testzeiten, Token-Kosten pro Brief, Refactoring, Prozess-Retro.
- **Neues Claude-Modell → Vergleichsrunde:** ein Referenz-Brief läuft mit altem und neuem Modell; Gate-Erfolg, Review-Befunde, Laufzeit und Tokens vergleichen; Empfehlung an mich.
- Verallgemeinerbare Prozessverbesserungen gehen als Vorschlag an die Vorlage (deren `ERFAHRUNGEN.md` bzw. `CHANGELOG.md`); ich entscheide.

### 3.3 Gate & Definition of Done
`scripts/gate.sh` (Linux) bzw. `scripts/gate.ps1` (Windows) ist grün, wenn **alle** Punkte erfüllt sind:
- [ ] CMake-Build Release + Debug ohne Warnungen in neuem Code (`-Werror` für `src/`)
- [ ] Unit- und Integrationstests grün (`ctest --output-on-failure`)
- [ ] Golden-Render-Tests (§11.2) und LUFS-Validierung (§11.3) grün, sobald vorhanden
- [ ] RealtimeSanitizer-Lauf ohne Befund (Linux/Clang, §11.5)
- [ ] clang-format/clang-tidy sauber, Secret-Scan sauber
- [ ] nur die im Brief genannten Dateien geändert
- [ ] Doku nachgezogen: `team/design/DESIGN.md`, README, `Projektinhalt.md`, `docs/commands.md`
- [ ] reviewer-Urteil `APPROVE`
- [ ] bei GUI-Änderungen: Screenshots unter `docs/screenshots/<karte>/`

`gate.yml` läuft bei jedem Push als Matrix Windows + Linux (§11.6).

**Definition of Done je Meilenstein:** alle Karten erledigt, CI-Artefakte für Windows + Linux vorhanden, Akzeptanztest (§12) dokumentiert, Prüfaufgabe (V) für mich in `team/TODO-PO.md`.

### 3.4 Agents: Modelle und Tracklab-Zusätze
Modell-IDs `[VERIFIZIEREN per /model und Doku]`: `claude-opus-5-5`, `claude-sonnet-5-5`, `claude-haiku-5-5`.

| Agent | Modell | Effort | Tracklab-Zusatz |
|---|---|---|---|
| researcher | sonnet-5-5 | medium | Quellenpflicht, Herstellerdoku vor Foren |
| test-writer | sonnet-5-5 | high | Tests aus den Akzeptanzkriterien; Golden-Fixtures nur per Brief ändern |
| implementer | sonnet-5-5; Audio-Thread/DSP/Plugin-Sandbox: opus-5-5 | high | Zusatzprüfungen: RTSan, Golden-Render, clang-tidy, nur eigene Dateien |
| reviewer | opus-5-5 | high | Pflichtfragen unten |
| cleanup | haiku-5-5 | low | Formatierung, tote Includes, Doku-Links |

Frontmatter je Agent u. a.: `model`, `effort`, `tools`, `isolation: worktree` (Pflicht für test-writer, implementer, cleanup), `maxTurns`.

**Pflichtfragen reviewer**
1. Ist der Code vom Audio-Thread erreichbar? Wenn ja: keine Allokation, keine Locks, kein IO, keine Exceptions (§6.3)?
2. Jeder neue Command: JSON-Schema mit `additionalProperties: false`, Undo/Redo symmetrisch, `destructive` richtig gesetzt?
3. Entspricht das Verhalten der referenzierten `DESIGN.md`-Revision und den Akzeptanzkriterien?
4. Neue Abhängigkeiten AGPLv3-kompatibel und dokumentiert?
5. Bei GUI: Screenshots vorhanden, Design-Tokens statt fest codierter Farben und Maße?

### 3.5 `USER INPUT REQUIRED`
Wenn du etwas von mir brauchst (Entscheidung, sudo, Hardwaretest, Prüfung, Tag-Push):
1. Eintrag in `team/TODO-PO.md` (dort stehen nur Entscheidungen und Prüfungen).
2. In `team/RESUME.md` den Status `BLOCKED: USER INPUT REQUIRED` mit Verweis setzen, committen, pushen.
3. Chat-Ausgabe in diesem Format (hat die Vorlage ein eigenes, gilt das), danach **stoppen**:
```
=== USER INPUT REQUIRED ===
Was: <eine Zeile>
Warum: <eine Zeile>
Aktion für David: <nummerierte, kopierbare Schritte/Befehle>
Antwort bitte: hier im Chat oder in team/TODO-PO.md
Ohne Antwort mache ich weiter mit: <nichts | Default X>
```

### 3.6 Claude-Code-Mechanik (`[VERIFIZIEREN]` gegen code.claude.com/docs)

| Mechanismus | Verwendung |
|---|---|
| `CLAUDE.md` | Abschnitt aus der Vorlage + Tracklab-Kurzregeln, Details per Verweis |
| Sub-Agents `.claude/agents/*.md` | aus der Vorlage, ergänzt nach §3.4 |
| Hooks `.claude/settings.json` | `PreToolUse` auf `Bash` → `scripts/hooks/guard-git.sh` (unter Windows über Git Bash) blockiert mit Exit 2 `git push`, `git merge`, `git checkout main` und Commits auf `main`, sobald die Session in einem Worktree läuft |
| `permissions.deny` | zusätzliche Sperren für Sub-Agents, falls Hooks nicht greifen |
| Git-Hooks | **kein** repo-lokales `core.hooksPath` (R6). Weitere Git-Hooks nur nach Entscheidung und nur so, dass die globalen Hooks weiterlaufen |
| Skills / Slash-Commands | nur wo die Vorlage sie nicht schon liefert, z. B. `render-check`, `screenshot-review` |
| Plan Mode | vor jeder Design-Revision und vor Merges mit Konflikten |
| „weiter“ | eine neue Session liest `team/RESUME.md` und macht weiter; nichts darf nur im Chat stehen |
| Agent Teams (experimentell) | **nicht verwenden**, außer ich entscheide es (bekannte Grenzen bei Resume, Koordination und Shutdown; die Mailbox liegt nicht im Repo) |

### 3.7 Teams je Scope und Testing
Eine Claude-Code-Session; du bist Lead für alle Scopes:

| Scope | Inhalt | ab |
|---|---|---|
| Research | `team/research/`: DAW-Funktionen, Standards, Libraries, Lizenzen | M0 |
| Product Build | `src/`, `tests/`: Engine, GUI, Claude-Integration | M1 |
| Testing | CI-Matrix, Golden Files, Smoke-Tests, Testprotokolle für meine Rechner | M1 |
| Marketing/Website | optional, später | ≥ 1.0 |

Übergaben zwischen Scopes über Inbox-Dateien, falls die Vorlage welche vorsieht, sonst über Board-Karten.

**Testing realistisch:** GitHub-Runner (`windows-latest`, `ubuntu-24.04`) prüfen Build, Unit-, Golden- und Headless-Tests, haben aber **keine Audio-Hardware**. Latenz-, ASIO-, PipeWire/JACK- und Mehrkanaltests laufen auf meinem Windows-Rechner und unter Linux Mint. Dafür schreibst du je Meilenstein ein Testprotokoll `docs/testing/manual/<meilenstein>.md` (Checkliste, erwartete Werte) und legst die Prüfung als V-Aufgabe an. VMs taugen nur für Installer- und Start-Smoke-Tests, nicht für Audio-Timing.

---

## 4. Produkt

### 4.1 Vision
Eine schnelle, stabile, schöne DAW für **Live-Mitschnitte, Band-Recording, Editing, Mixing und Mastering** unter Windows und Linux. Für einen Reaper-Nutzer fühlt sie sich sofort vertraut an (Actions, Shortcuts, Routing-Freiheit, Regionen und Render-Matrix). Sie übernimmt die besten Workflows der großen DAWs: die Mastering-Seite von Studio Pro, das Mixing von Cubase und Pro Tools, Lanes und Razor-Editing aus Reaper, die Modulation von Bitwig. Jede Aktion lässt sich gleichwertig per Maus, Tastatur, Controller, eingebautem Claude-Panel und externem MCP-Client ausführen. Tracklab ist **ohne Claude voll nutzbar**.

### 4.2 Nicht-Ziele (vorerst)
- **Kein Videoschnitt** – das macht DaVinci Resolve. Tracklab liefert meiner bestehenden Video-Pipeline die Audiodateien pro Song (Workflow A).
- Kein macOS, kein AU, kein AAX.
- Kein Notationsprogramm auf Dorico-Niveau, kein Live-Performance-Fokus im MVP, keine Cloud-Kollaboration.
- Keine eigene KI-Stem-Separation im MVP (später per Library, Lizenz klären).

### 4.3 Referenz-Workflows (= Akzeptanztests)

**A – Live-Mitschnitt aufbereiten (heutiger Bedarf, zuerst umsetzen)**
1. Stereo-Mitschnitt eines Konzertabends importieren: 2 Sets, rund 19 Songs, Kamera-Ton vom Mischpult, WAV oder MP3.
2. Ich füge die Setlist ein. Claude findet die Songgrenzen (Pausen, Applaus, Energieverlauf) und schlägt Regionen mit Tracknummer und Songname vor; ich bestätige oder korrigiere.
3. Trims sowie Fade-in und Fade-out pro Song.
4. Lautheit pro Song messen und auf −14 LUFS integriert bei −1 dBTP normalisieren.
5. Export pro Region als WAV 48 kHz/24-bit für die Video-Pipeline (DaVinci Resolve) und zusätzlich als MP3. Dateinamen aus Tracknummer und Songname, Messbericht je Datei.

**B – Multitrack-Bandaufnahme (Ausbau)**
1. Aufnahme mit 10–12 Eingängen gleichzeitig (Kick In/Out, Snare Top/Bottom, Hi-Hat, 2–3 Toms, 2 Overheads, Room) plus Bass-DI, Gitarre und Gesang; Monitoring, optional Metronom/Count-in, Loop-Takes.
2. Editing/Comping: Takes in Lanes, Swipe-Comping, phasenkohärentes Gruppen-Editing der Drums, Fades/Crossfades.
3. Mix: Drum-Ordner mit Drum-Bus, Parallelkompression per Send, Gruppen/VCA, Sidechain, Automation.
4. Mastering: Master-Kette, LUFS/True Peak/LRA, Referenztrack-Vergleich, Album-Reihenfolge.
5. Export: WAV 24-bit, FLAC und MP3, LUFS-Preset, Dither, Metadaten, Stems.

---

## 5. Tech-Stack: Vorschlag für ADR-001 (in Entscheidungsrunde 1 bestätigen)

| Baustein | Empfehlung | Stand/Lizenz `[VERIFIZIEREN]` |
|---|---|---|
| Sprache/Build | C++20, CMake ≥ 3.25, Ninja; MSVC (Windows), Clang + GCC (Linux) | Tracktion Engine verlangt C++20 |
| Framework | JUCE 8 (zuletzt 8.0.15, Juli 2026) | AGPLv3 oder kommerzielle JUCE-Lizenz |
| Audio-/Edit-Engine | Tracktion Engine v3.x (v3.2.0 mit Automationsmodi Read/Write/Touch/Latch; seit v3 Clip-Launcher) | GPLv3 oder kommerziell, getrennt von JUCE lizenziert |
| Plugin-Formate | VST3 (MVP); LV2 unter Linux (v1, über JUCE-LV2-Hosting); CLAP (v1, eigenes Hosting mit CLAP-SDK + clap-helpers, Startpunkt ggf. `juce_clap_hosting`) | VST3-SDK 3.8 unter MIT; CLAP unter MIT; JUCE 9 mit CLAP ist angekündigt, aber noch nicht erschienen und bringt laut Roadmap nur CLAP-**Authoring** |
| Audio-Backends | Windows: ASIO (SDK jetzt auch unter GPLv3) + WASAPI. Linux: ALSA + JACK-API (läuft unter PipeWire über pipewire-jack) | Linux Mint 22.x nutzt PipeWire als Standard |
| GUI | native JUCE-Komponenten + eigenes Design-Token-System; unter Windows Direct2D-Renderer (JUCE 8), unter Linux Software- bzw. OpenGL-Rendering. WebView nur als Option fürs Claude-Panel (per ADR) | JUCE-8-WebView: Windows WebView2, Linux WebKitGTK (zusätzliche Abhängigkeiten) |
| HTTP/TLS für die Claude-API | libcurl (SSE-Streaming) oder JUCE `WebInputStream`, Wahl per ADR | – |
| JSON / Schema | nlohmann/json + JSON-Schema-Validierung | MIT |
| Tests | Catch2 oder GoogleTest; CTest | – |
| Keychain | Windows Credential Manager; Linux Secret Service (libsecret) | – |

**Lizenzfolge (für ADR-001):** JUCE unter AGPLv3 + Tracktion Engine unter GPLv3 → Tracklab wird unter **AGPLv3** lizenziert (GPLv3 und AGPLv3 sind kombinierbar). Ein Closed-Source-Vertrieb würde kommerzielle Lizenzen für **beide** erfordern. VST3 (MIT) und ASIO (GPLv3-Option) sind kompatibel. Bleibt das Repo privat, greifen die AGPL-Pflichten erst bei einer Weitergabe.

**Name:** „Tracklab“ (frühere Arbeitstitel: OpenDAW, Mojo DAW). Ähnliche Namen gibt es: „Tracklab“ von 2Simple (Musik-Lernwerkzeug für Schulen), Sonys VR-Spiel „Track Lab“ und den Sample-Dienst Tracklib. Für das private Repo unkritisch; vor einer Veröffentlichung Markenprüfung (EUIPO) in `team/research/lizenz-und-name/`.

**Kurz bewertete Alternativen (in ADR-001 dokumentieren):**

| Alternative | Pro | Contra | Urteil |
|---|---|---|---|
| Ardour-Fork (GPLv2+, C++/GTK) | riesiger Funktionsumfang, LV2/VST3, Lua, Mackie Control, AAF-Import | GTK-Oberfläche schwer „schön“ zu machen, große fremde Codebasis, Claude-Integration quer durch alles | als **Funktionsreferenz** nutzen, nicht forken |
| Rust-Stack (cpal, eigene Engine, egui/iced/vizia, CLAP über clack, MCP über das offizielle `rmcp`) | Speichersicherheit, offizielles MCP-SDK | Engine, Editing und Plugin-Hosting (VST3/LV2) fast komplett selbst bauen; Jahre Rückstand | nicht für v1 |
| JUCE ohne Tracktion Engine | volle Kontrolle | Edit-Modell, Clips, Automation, Rendering selbst bauen | nur falls die Tracktion Engine im Spike scheitert |

**M0-Spike (Pflicht vor Annahme von ADR-001):** Minimal-App mit JUCE + Tracktion Engine: Stereo-WAV/MP3 importieren, eine Region offline rendern und ihre Lautheit messen (Workflow A), 12 Eingänge aufnehmen (Dummy-Device in CI), ein VST3 laden; Build unter Windows + Linux in CI. Ergebnis in `team/research/engine-spike/`.

---

## 6. Architektur

### 6.1 Module

| Modul | Verantwortung |
|---|---|
| `core` | Datenmodell-Wrapper, IDs, Undo/Redo, Command-Registry, Event-Bus |
| `engine` | Tracktion-Engine-Adapter, Transport, Aufnahme, Wiedergabe, Rendering, PDC |
| `io` | Audio-/MIDI-Geräte, Backend-Auswahl, Latenzmessung (Loopback) |
| `plugins` | Scanner (eigener Prozess), Hosting VST3/LV2/CLAP, Sandbox, Presets, FX-Chains |
| `dsp` | eigene Prozessoren: Meter, LUFS, Dither, Songgrenzen-Analyse, Basis-FX |
| `project` | Projektformat, Autosave, Backups, Templates, Import/Export |
| `ui` | Views, Theme/Design-Tokens, Shortcut-Presets, Screensets |
| `assistant` | Claude-Client (REST/SSE), Tool-Generierung, Turn-Transaktionen, Kosten |
| `mcp` | lokaler MCP-Server (Streamable HTTP) + stdio-Shim `tracklab-mcp` |
| `cli` | `tracklab-cli`: render, analyze, find-songs, run-commands, screenshot, validate |

### 6.2 Command-Registry (Herzstück)
- **Jede** Aktion ist ein `Command` mit: `id` im Namespace-Stil (`track.create`), Titel (Deutsch), Beschreibung (Englisch, für Claude und Tool Search), JSON-Schema für die Parameter (`additionalProperties: false`), Rückgabe-Schema, Flags `readOnly | undoable | destructive | longRunning`, Standard-Shortcut, Menüpfad.
- GUI, Menüs, Shortcuts, Controller, Action-Liste, Makros, Claude-Panel, MCP und CLI rufen **dieselbe** Registry auf. Es gibt keinen zweiten Codepfad.
- Makros/Custom Actions (Reaper-Stil) sind Listen von Commands mit Parametern und werden selbst als Command registriert.
- Commands laufen auf dem Message-Thread. Änderungen am Engine-Graph gehen über lock-freie Übergabe an den Audio-Thread.
- Die Registry erzeugt `docs/commands.md` und `tools.json` automatisch; die CI prüft, ob beide aktuell sind.

### 6.3 Echtzeit-Regeln (`docs/realtime.md`, MUSS)
- Im Audio-Thread **keine** Allokation, keine Locks/Mutex, kein IO, kein Logging außer in lock-freie Ringpuffer, keine Exceptions, keine blockierenden Systemaufrufe.
- Kommunikation über SPSC/MPSC-Lock-free-Queues und atomare Snapshots; Speicher wird auf einem Hintergrund-Thread freigegeben.
- RealtimeSanitizer (Clang `-fsanitize=realtime`, `[[clang::nonblocking]]`) in der CI unter Linux `[VERIFIZIEREN: Clang-Version]`.
- Plugin-Sandbox: Ein abstürzendes Plugin darf den Audio-Thread nicht mitreißen (§6.5).

### 6.4 Datenmodell & Projektformat
- Tracktion-Edit (ValueTree/XML) als internes Modell. Projektordner: `<Projekt>/<Projekt>.<ext>` (Endung per ADR) plus `Audio/`, `Renders/`, `Backups/`, `Peaks/`, `claude-log.jsonl`.
- Autosave (Intervall einstellbar), rotierende Backups (N Versionen), Crash-Recovery-Datei.
- Relative Pfade, „Projekt sammeln & kopieren“.
- Formatversion im Header, Migrationstests.

### 6.5 Plugin-Hosting & Crash-Schutz
- Scanner als eigener Prozess, Blacklist für abstürzende Plugins, Scan-Cache.
- Sandbox-Modi nach dem Vorbild von Bitwig (seit 2.5: „Within Bitwig“, „Together“ [Standard], „By manufacturer“, „By plug-in“, „Individually“; „Individually“ ist laut Bitwig am speicherintensivsten, aber am sichersten). Bei einem Absturz wird das Plugin gebypasst und die Spur markiert; „Plugin neu laden“ bzw. „Alle Plugins neu laden“ wird angeboten. Das Projekt bleibt offen.
- Latenz-Reporting → PDC. Presets: format-eigene und eigene; FX-Chains und Container (Reaper-7-Stil) mit Makro-Parametern.

---

## 7. Claude-Integration

### 7.1 Assistant-Panel
- Andockbares Panel (rechts), Streaming-Antworten (SSE), Abbrechen-Button.
- **Modi:** `Fragen` (jede schreibende Aktion bestätigen) · `Automatisch` (nur destruktive bestätigen) · `Nur vorschlagen` (Claude plant, nichts wird ausgeführt).
- **Plan-Vorschau:** Vor schreibenden Aktionen ruft Claude `assistant.propose_plan` auf. Die GUI zeigt die geplanten Commands mit Parametern und betroffenen Spuren, dazu „Ausführen / Bearbeiten / Verwerfen“.
- Kontext-Chips: Die aktuelle Auswahl (Spuren, Clips, Zeitbereich) geht automatisch mit.
- Verlauf pro Projekt, abschaltbar; Audit-Log aller ausgeführten Commands in `claude-log.jsonl`.

### 7.2 Sicherheit beim Ausführen
- **Jeder Claude-Turn = eine Undo-Transaktion** („Claude: <Kurzbeschreibung>“); ein Strg+Z macht den ganzen Turn rückgängig.
- `destructive`-Commands (Löschen, Überschreiben von Dateien, Render über eine bestehende Datei, Plugin mit Zustand entfernen) brauchen in jedem Modus eine Bestätigung.
- Limits: höchstens 50 Commands pro Turn (einstellbar), Timeouts, Abbruch jederzeit.
- **Keine Shell, kein beliebiger Dateizugriff:** nur der Projektordner und in den Einstellungen freigegebene Ordner (Samples, Renders, Referenztracks, Mitschnitte).

### 7.3 Tool-Design (Claude Messages API)
- Direkter REST-Aufruf, weil es kein offizielles C++-SDK gibt: `POST https://api.anthropic.com/v1/messages`, Header `x-api-key`, `anthropic-version: 2023-06-01`, `content-type: application/json`, `stream: true` (SSE).
- Tools werden **automatisch aus der Command-Registry erzeugt**: `name` = Command-ID (`track.create` → `track_create`, falls Punkte im Tool-Namen nicht erlaubt sind `[VERIFIZIEREN: erlaubtes Namensmuster]`), `description`, `input_schema`, **`strict: true`** (verlangt `additionalProperties: false`).
- **Tool Search:** `tool_search_tool_bm25_20251119` (oder `_regex_`) plus `defer_loading: true` für alle Tools außer 3–5 Kern-Tools: `project.get_state`, `selection.get`, `assistant.propose_plan`, `transport.control`, `analyze.loudness`. Das Tool-Search-Tool selbst wird nie deferred. Namespaces und Beschreibungen sind auf Auffindbarkeit optimiert („track.* = track management: create, delete, rename, color, folder …“).
- **Prompt Caching:** `cache_control` auf den System-Prompt und auf das letzte **nicht deferred** Tool. Deferred Tools tragen kein `cache_control`; laut Doku bleibt der Cache mit `defer_loading` erhalten.
- **Effort-Parameter** für Plan- und Analyse-Anfragen `[VERIFIZIEREN: Name, Werte, Modellunterstützung]`.

### 7.4 Kontext-, State- und Analyse-Tools

| Tool | Zweck |
|---|---|
| `project.get_state` | kompakte Zusammenfassung: Tempo, Taktart, Länge, Spurbaum (Ordner/Busse), Marker, Regionen, Master-Kette |
| `track.list`, `track.get` | Details inkl. Eingänge, Sends, FX, Gain, Pan, Mute/Solo |
| `mixer.get_routing` | Routing-Graph (Busse, Sends, Sidechains) |
| `selection.get` / `selection.set` | Auswahl lesen/setzen |
| `analyze.loudness` | Integrated, Short-term max, Momentary max, LRA, True Peak (Bereich, Spur oder Master, offline gerendert) |
| `analyze.find_song_boundaries` | Songgrenzen-Vorschläge aus Stille, Applaus und Energieverlauf; optional mit Setlist (Anzahl, Namen) sowie Mindest- und Höchstlänge |
| `analyze.spectrum` | 1/3-Oktav-Bänder, Vergleich gegen einen Referenztrack |
| `analyze.peaks` / `analyze.clipping` / `analyze.phase` | Pegel, Übersteuerungen, Korrelation (z. B. Overheads gegen Kick) |
| `analyze.compare_reference` | Lautheit und Spektrum: Mix gegen Referenz |
| `analyze.verify_change` | Messung vor und nach einer Aktion (Muster aus reaper-daemon) |

Claude „hört“ ausschließlich über Messwerte. Das Panel zeigt sie an, damit ich nachvollziehen kann, worauf Claude reagiert.

### 7.5 Modellwahl & Kosten
- Standard im Panel: `claude-sonnet-5-5` (Latenz, Kosten, starkes Tool Use). Umschaltbar: `claude-opus-5-5` für Mix-Analyse und Planung; `claude-haiku-5-5` für schnelle Ein-Schritt-Befehle **nur mit kleinem, nicht deferred Toolset**, falls Haiku Tool Search nicht unterstützt `[VERIFIZIEREN]`.
- Kosten- und Token-Anzeige pro Turn und Session aus `usage` (input, output, cache read/creation). Preise stehen in einer editierbaren Tabelle (`presets/pricing.json`), nicht im Code.

### 7.6 Schlüssel & Offline
- API-Key nur im Schlüsselbund des Betriebssystems (Windows Credential Manager / libsecret); kein Klartext-Fallback ohne ausdrückliche Zustimmung.
- Ohne Netz oder Key zeigt das Panel „offline“; Tracklab funktioniert vollständig weiter.

### 7.7 MCP-Server (Steuerung durch Claude Desktop / Claude Code)
- **Opt-in** (standardmäßig aus), nur `127.0.0.1`, zufälliges Bearer-Token (in den Einstellungen anzeigen/erneuern), `Origin`-Header prüfen (Schutz gegen DNS-Rebinding), Rate-Limit, Audit-Log.
- Transport: **Streamable HTTP** nach aktueller MCP-Spezifikation (2025-11-25; Revision 2026-07-28 prüfen `[VERIFIZIEREN]`). Zusätzlich ein kleines **stdio-Shim** `tracklab-mcp`, das Claude Desktop startet und das sich mit dem Token zur laufenden Tracklab verbindet.
- Ein offizielles MCP-SDK für C++ gibt es nicht (offiziell: TypeScript, Python, C#, Go, Java, Rust, Swift, Ruby, PHP, Kotlin). Daher eine schlanke eigene JSON-RPC-Implementierung im Modul `mcp` mit Conformance-Tests; Alternative per ADR: Shim in Rust mit dem offiziellen `rmcp`.
- Tools = dieselbe Registry (gleiche Namen, gleiche Schemas) und dieselben Sicherheitsregeln (§7.2); destruktive Aktionen bestätige ich in der Tracklab-GUI.
- README-Anleitung: `claude mcp add --transport http tracklab http://127.0.0.1:<port>/mcp --header "Authorization: Bearer <token>"` sowie die Claude-Desktop-Konfiguration `[VERIFIZIEREN: aktuelle CLI-Syntax]`.
- Designreferenz bestehende Reaper-MCP-Server (in `team/research/mcp-server/` auswerten; welchen ich selbst nutze, kläre ich in Runde 1): Web-Interface-Bridge (DevWesC), python-reapy mit 58 Tools (bonfire/itsuzef), Lua-Datei-Bridge mit 80 Tools und JSFX-Analysern (mthines), `eval_lua` + `read_rpp` als geschlossener Prüfkreis (Oisub), `scan_fx`/`verify_change` mit Undo-Blöcken (reaper-daemon). Übernehmen: Undo-Block je Aktion, Vorher/Nachher-Messung, lesbarer Projektzustand. Nicht übernehmen: freie Skriptausführung (`eval_lua`-Äquivalent), weil sie §7.2 widerspricht.

### 7.8 Beispiel-Befehle (als Akzeptanztests in `tests/assistant/`)

| Befehl an Claude | Erwartete Tool-Folge (kurz) |
|---|---|
| „Hier ist die Setlist von gestern (19 Songs). Schneid den Mitschnitt in Songs.“ | `import.audio` → `analyze.find_song_boundaries(setlist)` → `assistant.propose_plan` → `marker.add_region` ×19 |
| „Jeder Song bekommt 2 s Fade-in und 4 s Fade-out, dann alles auf −14 LUFS.“ | `clip.set_fade` → `export.set_normalize(preset="video")` |
| „Rendere alle Songs als WAV 48/24 fürs Video und zusätzlich als MP3 320.“ | `export.add_job(range="regions", format="wav48_24")` → `export.add_job(range="regions", format="mp3_320")` → `export.render_queue.run` |
| „Leg alle Drum-Spuren in einen Ordner mit Drum-Bus und Parallelkompression.“ | `track.list` → `track.create_folder(name="Drums")` → `track.move` → `track.create_bus("Drum Bus")` → `route.set_output` → `track.create_bus("Drum Crush")` → `route.add_send` → `fx.add(compressor, preset="parallel-heavy")` |
| „Benenne die Spuren nach Eingang: In1 Kick In, In2 Kick Out … und färbe die Drums rot.“ | `track.rename` ×n → `track.set_color` |
| „Arme Spuren 1–12, Monitoring auto, 4 Takte Count-in.“ | `rec.arm` → `rec.set_monitoring` → `transport.set_countin` |
| „Mach aus den drei Gesangstakes einen Comp: jeweils die gleichmäßigste Phrase.“ | `take.list` → `analyze.loudness` (pro Take/Phrase) → `assistant.propose_plan` → `comp.select_range` |
| „Quantisiere Snare und Kick phasenkohärent auf 1/16, nur Transienten über −20 dB.“ | `edit.detect_transients(group)` → `edit.audio_quantize` |
| „Wo clippt der Mix? Senk die betroffenen Spuren so weit, dass 1 dB Headroom bleibt.“ | `analyze.clipping` → `mixer.set_volume` |
| „Vergleiche meinen Master mit referenz.wav: Lautheit und Tiefen.“ | `analyze.compare_reference` |

---

## 8. GUI/UX
- **Views** nach Vorbild der großen DAWs: Arrange (Timeline mit Lanes), Mixer (Konsole), Editor (Audio-/Sample-Editor, Piano-Roll, Drum-Editor), Browser (Dateien, Plugins, Presets, Projekte), Inspector (Spur-/Clip-Eigenschaften), Transportleiste, Region-Manager, Mastering-Seite, Claude-Panel, Render-Queue.
- **Design-Tokens** (`themes/*.json`): Farben, Abstände, Radien, Typografie, Meter-Farben. Dark (Standard) und Light, Wechsel ohne Neustart.
- **HiDPI:** Skalierung 100–300 %, Vektor-Icons (SVG), unter Windows der Direct2D-Renderer.
- **Screensets/Workspaces** (Reaper/Cubase-Stil), andockbare Fenster.
- **Shortcut-Presets:** Tracklab-Standard und **Reaper-kompatibel** (Hauptbelegungen; Import einer `reaper-kb.ini` später `[VERIFIZIEREN]`), danach Pro-Tools- und Cubase-Stil. Maus-Modifier-Sets wie in Reaper 7.
- **Branding:** Logo und App-Icon liegen fertig vor (Vault `Tracklab/branding/`, im Repo `assets/branding/`, siehe dortige `README.md`). Die Farben daraus sind die Basis der Design-Tokens: Akzent `#F59E0B` (Playhead, Auswahl), Clips `#38BDF8` / `#2DD4BF` / `#818CF8`, Hintergrund `#0F172A`–`#1E2A47`. App-Icon unter Windows `tracklab.ico` (EXE + Installer), unter Linux `png/tracklab-icon-256.png`/`-512.png` (AppImage/.deb). Logo in README, Splash und About-Dialog. Änderungen am Logo nur per Entscheidung; PNG/ICO immer mit `render-icons.py` aus den SVGs erzeugen.
- **Action-Liste** mit Suche über alle Commands, Custom Actions und Makros.
- Barrierefreiheit: vollständig per Tastatur bedienbar, Screenreader über JUCE-Accessibility `†`, Kontrastmodus, skalierbare Schrift.
- **Screenshot-Prüfung:** `tracklab-cli screenshot --project <p> --view mixer --theme dark --scale 2 --out <png>` rendert offscreen; die Linux-CI nutzt dafür Xvfb. Jede GUI-Karte liefert Vorher/Nachher-Screenshots; der reviewer prüft sie gegen das Design, ich bekomme sie zur Prüfung.

---

## 9. Feature-Katalog (konsolidiert, dedupliziert)

Prioritäten: **MVP** = Kernfunktionen für Workflow A und die Grundzüge von Workflow B · **v1** = vollwertige DAW bis 1.0 (Workflow B komplett) · **v2** = Komfort/Erweiterung · **später** = optional. Tool-Namen sind Vorschläge (Namespace = Tool-Search-Gruppe).

### 9.1 Projekt & Session

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Neues Projekt, Templates (z. B. „Mitschnitt Stereo“, „Band 12-Kanal“) | alle | MVP | `project.new`, `project.save_as_template` |
| Autosave, rotierende Backups, Crash-Recovery | Reaper† | MVP | `project.save`, `project.restore_backup` |
| Projekt-Tabs (mehrere Projekte offen) | Reaper | v1 | `project.open`, `project.switch` |
| Start-Hub mit Projekt-Vorschau (kurzes Audio-Preview) | Cubase 15 Hub | v2 | `project.list_recent` |
| Snapshots/Versionen eines Projekts | Ardour Snapshots | v1 | `project.snapshot`, `project.list_snapshots` |
| Subprojekte (Projekt als Clip) | Reaper† | später | `project.insert_subproject` |
| Projekt sammeln/konsolidieren, relative Pfade | Reaper (relative Pfade 7.77) | v1 | `project.consolidate` |
| Projektnotizen | Reaper ($notes-Wildcard 7.11) | v1 | `project.set_notes` |

### 9.2 Spuren & Spurverwaltung

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Audio-, MIDI/Instrument-, Bus/FX-Spuren | alle | MVP | `track.create`, `track.delete`, `track.duplicate` |
| Ordnerspuren (verschachtelt, als Bus nutzbar) | Reaper | MVP | `track.create_folder`, `track.move` |
| Gruppen (Gain/Mute/Solo/Edit-Verknüpfung) | Pro Tools/Cubase† | MVP | `track.group_create`, `track.group_set_params` |
| VCA-Fader | Cubase/Pro Tools/Studio Pro† | v1 | `track.create_vca`, `track.assign_vca` |
| Spur-Templates/Presets inkl. FX und Routing | Reaper, Cubase Track Presets | v1 | `track.save_template`, `track.load_template` |
| Lanes: Takes/Playlists/Track-Versionen | Reaper 7 Track Lanes, Pro Tools Playlists, Cubase Track Versions | MVP | `take.list`, `lane.create`, `lane.set_active` |
| Farben (Palette), Icons, Höhe, Ein-/Ausblenden | Reaper 7.81 Farbpalette | MVP | `track.set_color`, `track.set_height`, `track.set_visible` |
| Track Spacer (visuelle Trenner) | Reaper 7 | v2 | `track.insert_spacer` |
| Track Pin (Spur beim Scrollen fixieren) | Pro Tools 2026.4 | v2 | `track.pin` |
| Mehrkanal-Spuren (bis 128 Kanäle) | Reaper 7 | v2 | `track.set_channels` |

### 9.3 Aufnahme

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Multitrack-Aufnahme mit 12+ Eingängen, Input-Zuordnung, Benennung nach Eingang | alle | MVP | `rec.arm`, `rec.set_input`, `track.rename` |
| Input-Monitoring (aus/an/auto), Latenzanzeige | Tracktion Engine (MonitorModes) | MVP | `rec.set_monitoring` |
| Punch-In/Out, Pre-Roll | alle; Tracktion Engine v3 | MVP | `rec.set_punch`, `transport.set_preroll` |
| Loop-/Take-Recording in Lanes | Reaper 7, Logic Take Folders | MVP | `rec.set_mode(loop_takes)` |
| Metronom (Sounds, Akzent, nur bei Aufnahme), Count-in | alle | MVP | `transport.set_metronome`, `transport.set_countin` |
| Retrospective Record (Audio/MIDI) | Logic Flashback Capture, FL Studio Audio Logger | v2 | `rec.capture_retro` |
| Aufnahme-Latenzkompensation per Loopback-Messung | Reaper†/Cubase† | MVP | `io.measure_latency` |
| FX beim Arm automatisch bypassen | Reaper 7.35 | v2 | `rec.set_auto_bypass_fx` |

### 9.4 Audio-Editing

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Split, Trim, Move, Copy, Snap/Grid, Zoom | alle | MVP | `clip.split`, `clip.trim`, `clip.move`, `edit.set_grid` |
| Dynamic Split (an Stille/Transienten teilen) | Reaper† | MVP | `edit.dynamic_split` |
| Razor-/Range-Editing über mehrere Spuren | Reaper Razor Edits† | MVP | `edit.razor_select`, `edit.razor_delete` |
| Ripple-Modi (pro Spur / alle Spuren, Kanten-Trim) | Reaper (Ripple Edge Trim 7.35) | v1 | `edit.set_ripple` |
| Swipe-Comping aus Lanes, A/B mehrerer Comps | Reaper 7, Logic Quick Swipe | MVP | `comp.select_range`, `comp.new` |
| Fades/Crossfades mit Kurven, Crossfade-Editor | Reaper (7.81 neue Fade-Aktionen) | MVP | `clip.set_fade`, `clip.crossfade` |
| Clip-Gain/Item-Volume, Clip-Gain-Hüllkurve | Pro Tools† | MVP | `clip.set_gain` |
| Gruppen-Editing phasenkohärent (Drums) | Pro Tools/Cubase† | v1 | `edit.group_edit` |
| Transientenerkennung, Audio-Quantize | Logic Flex Time, Cubase AudioWarp† | v1 | `edit.detect_transients`, `edit.audio_quantize` |
| Time-Stretch/Warp, Tempo-Erkennung | Logic Smart Tempo, Tracktion Engine Time-Stretch | v1 | `clip.set_stretch`, `tempo.detect` |
| Pitch-Shift, Pitch-Kurven auf Clips | Studio Pro 8.1 Pitch Curves | v2 | `clip.set_pitch` |
| Region-/Clip-FX (Effekte pro Clip) | Ardour 9 Region FX | v1 | `clip.add_fx` |
| Spektralansicht/-reparatur, Klick-Entfernung | Reaper 7.75 Spectral Repair, 7.81 Repair Pops/Clicks | v2 | `edit.spectral_repair`, `edit.repair_clicks` |
| ARA-2-Hosting (Melodyne u. a.) | Reaper, Studio Pro, Pro Tools | v2 | `clip.open_ara` |
| Item-Locking, Clip-Gruppen | Reaper (Item Locking 7.6x) | v1 | `clip.lock`, `clip.group` |

### 9.5 MIDI

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Piano-Roll (Velocity, CC-Lanes, Quantize) | alle | v1 | `midi.insert_notes`, `midi.quantize` |
| Drum-Editor mit Notennamen-Maps | Cubase†, Ardour MIDNAM | v1 | `midi.set_drum_map` |
| Drum-Trigger aus Audio-Transienten (Drum-Replacement) | – | v2 | `midi.from_transients` |
| Step-Sequencer/Pattern-Editor | FL Studio, Cubase 15 Pattern Editor | v2 | `midi.pattern_create` |
| Skalen/Projekt-Tonart, Akkordspur | Bitwig 6 Projekt-Key, Studio Pro/Cubase Chord Track | v2 | `midi.set_scale`, `chord.set` |
| MIDI-Generatoren/Transformationen | Live 12 MIDI Tools | später | `midi.generate` |
| MPE | Studio One 5+, Bitwig† | später | – |
| Notation | Studio Pro Score Editor, Cubase 15 | später | – |

### 9.6 Arrangement & Tempo

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Marker, Regionen, Region-Manager | Reaper | MVP | `marker.add`, `marker.add_region`, `marker.list` |
| Regionen aus Setlist, Songgrenzen-Erkennung (Claude) | Tracklab-eigen | MVP | `analyze.find_song_boundaries`, `marker.regions_from_setlist` |
| Tempo-/Taktart-Spur (Rampen) | alle; Tracktion Engine Tempo-Kurven | MVP | `tempo.set`, `tempo.add_point`, `tempo.set_timesig` |
| Arranger-Spur (Songteile verschieben/duplizieren) | Studio Pro, Cubase Arranger† | v1 | `arrange.move_section`, `arrange.duplicate_section` |
| Clip-Launcher/Szenen | Ableton Live Session View, Bitwig, Tracktion Engine v3, Ardour 9 Cues | später | `launcher.trigger` |
| Clip-Aliase (verknüpfte Wiederholungen) | Bitwig 6 | später | – |

### 9.7 Mixing & Routing

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Konsole: Fader, Pan, Mute/Solo, Meter (Peak/RMS/LUFS) | alle | MVP | `mixer.set_volume`, `mixer.set_pan`, `mixer.set_mute`, `mixer.set_solo` |
| Inserts (Slots), Sends pre/post, Busse | alle; Reaper 7.75 Mixer-Slots | MVP | `fx.add`, `route.add_send`, `track.create_bus` |
| Sidechain (MIDI-Send wird automatisch abgeschaltet) | Reaper 7.77 | v1 | `route.add_sidechain` |
| Routing-Matrix | Reaper | v1 | `route.matrix_get`, `route.matrix_set` |
| Automatische Plugin-Latenzkompensation (PDC) | alle | MVP | – (Engine) |
| Parallele FX und FX-Container mit Makros | Reaper 7 | v1 | `fx.create_container`, `fx.set_parallel` |
| Kanalübersicht | Studio Pro 8 | v2 | `mixer.get_channel` |
| Mixer-Strips zwischen Projekten im-/exportieren | Ardour 9 | v2 | `mixer.export_strip`, `mixer.import_strip` |
| Modulatoren (LFO, Random, S&H …) auf Parameter | Cubase 14/15, Bitwig | v2 | `mod.add`, `mod.assign` |
| A/B-Vergleich pro Plugin | Live 12.3 | v1 | `fx.ab_toggle` |
| Control Room/Listen-Bus, Cue-Mixe für die Band | Cubase† | v2 | `monitor.create_cue_mix` |
| Phase/Mono pro Kanal | – | v1 | `mixer.set_phase`, `mixer.set_mono` |

### 9.8 Automation

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Automations-Lanes für alle Parameter | alle | MVP | `auto.show_lane`, `auto.add_points` |
| Modi Read/Touch/Latch/Write | Pro Tools/Cubase; Tracktion Engine 3.2 | v1 | `auto.set_mode` |
| Trim-Modus | Pro Tools† | v2 | `auto.set_mode(trim)` |
| Kurvenformen, Freihand mit Kurven-Fitting | Bitwig 6 | v1 | `auto.set_curve` |
| Automation per Tastatur bearbeiten | Ardour 9 | v2 | – |
| Clip-/Item-Automation, Automation-Clips | Bitwig 6, Studio Pro† | v2 | `auto.clip_create` |

### 9.9 Plugin-Hosting

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| VST3 hosten | alle (VST3-SDK unter MIT) | MVP | `plugin.scan`, `fx.add` |
| LV2 hosten (Linux) | Ardour; JUCE-LV2-Host | v1 | – |
| CLAP hosten | Reaper (seit 6.71), Bitwig | v1 | – |
| Scanner im eigenen Prozess + Sandbox, Absturz ohne Projektverlust | Bitwig | v1 | `plugin.set_sandbox_mode` |
| Plugin-Manager: Favoriten, Kategorien, „im Projekt verwendet“ | Cubase 15 | v1 | `plugin.list`, `plugin.search` |
| Presets laden/speichern, FX-Chains | Reaper† | MVP | `fx.load_preset`, `fx.save_chain` |
| Parameter in echten Einheiten setzen (z. B. „−2,5 dB“) | reaper-daemon (Designreferenz) | MVP | `fx.set_param`, `fx.get_params` |

### 9.10 Mitgelieferte Effekte & Instrumente (minimal, eigene oder lizenzkompatible)

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| EQ (parametrisch, mit Analyzer), Kompressor, Gate, Limiter (True Peak) | Reaper ReaEQ/ReaComp†, Studio Pro Pro EQ | MVP | `fx.add(type=…)` |
| Reverb, Delay, Sättigung, Transient Shaper | Studio Pro Studio Verb, Cubase UltraShaper | v1 | `fx.add` |
| Tuner, Phasen-/Korrelationsmeter, Spektrum | Bitwig 6.1 Tuner, Ardour 9 Analyzer | v1 | `analyze.spectrum` |
| Sampler/Drum-Sampler (Trigger) | Reaper ReaSamplOmatic5000, Studio Pro Impact | v2 | – |
| Amp-Simulationen | Studio Pro 8 Mustang/Rumble | später (Plugins Dritter) | – |

### 9.11 Mastering & Analyse

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| LUFS-Meter (M/S/I), LRA, True Peak (ITU-R BS.1770 / EBU R128) | Reaper Loudness Meter†, alle | MVP | `analyze.loudness` |
| Mastering-Seite: Songs in Reihenfolge, Pausen, Master-Kette, Album-Lautheit | Studio Pro Project-Seite† | v1 | `master.album_add_song`, `master.set_gap` |
| Referenztrack-A/B mit Lautheitsabgleich | gängige Plugins | v1 | `master.add_reference`, `analyze.compare_reference` |
| Mastering-Assistent (Vorschlag für Kette/EQ/Lautheit, Claude-basiert) | Logic Mastering Assistant | v2 | `master.assist` |
| Dither (TPDF, optional Noise Shaping) | alle | MVP | `export.set_dither` |
| DDP-Export | Studio Pro†, Reaper† | später | `export.ddp` |
| ISRC/CD-Text | Studio Pro† | später | `master.set_metadata` |

### 9.12 Export & Import

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Render-Dialog: Bereich (Projekt/Zeitauswahl/Regionen), Format, Samplerate, Bittiefe | Reaper | MVP | `export.render` |
| Render pro Region (jede Region als eigene Datei) | Reaper | MVP | `export.render(range="regions")` |
| Render-Queue | Reaper | MVP | `export.add_job`, `export.render_queue.run` |
| Region-Render-Matrix (Region × Spur/Bus) | Reaper | v1 | `export.region_matrix.set` |
| Stems (Spuren, Busse, Master), Bounce-in-Place | Reaper†, Live 12.3 Bounce Group | MVP | `export.stems`, `track.bounce_in_place` |
| Dateinamen-Wildcards (z. B. Tracknummer + Regionsname) | Reaper | MVP | `export.set_pattern` |
| LUFS-Normalisierung beim Render + Plattform-Presets | Reaper† | MVP | `export.set_normalize` |
| Metadaten (BWF, ID3, Vorbis Comments) | Reaper† | v1 | `export.set_metadata` |
| DAWproject Import/Export | Bitwig, Studio Pro, Cubase | v1 | `import.dawproject`, `export.dawproject` |
| MIDI (SMF) Import/Export | alle | v1 | `import.midi`, `export.midi` |
| AAF-Import | Ardour 9 (verbessert), Pro Tools† | v2 | `import.aaf` |
| RPP-Import (Reaper, Klartextformat) | – | v2 | `import.rpp` |
| Audio-Import mit Samplerate-Konvertierung, Stretch-Optionen | Reaper 7.77 | MVP | `import.audio` |

### 9.13 Workflow & Erweiterbarkeit

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Action-Liste mit Suche, Custom Actions/Makros | Reaper | MVP | `action.search`, `action.run`, `action.create_macro` |
| Shortcut- und Maus-Modifier-Sets, Reaper-Preset | Reaper 7 | MVP | `prefs.set_shortcut_preset` |
| Key Commands zusammenführen/importieren | Logic 12.4 | v2 | – |
| Screensets/Workspaces | Reaper† | v1 | `view.save_screenset`, `view.load_screenset` |
| Themes (Dark/Light, Token-Editor) | Reaper Theme Adjuster | MVP | `ui.set_theme` |
| Skripting (Lua) nur über die Command-Registry | Reaper ReaScript, Ardour Lua | v2 | `script.run` (Sandbox, nicht für Claude freigegeben) |
| Batch-Regeln (Filter → Aktion) wie der Logical Editor | Cubase Logical Editor† | später | `edit.batch_rule` |

### 9.14 Performance

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Multicore-Graph, CPU-Anzeige pro Spur/Plugin | alle; Tracktion Engine CPU-Metriken | MVP | `perf.get_stats` |
| Freeze/Unfreeze | alle | v1 | `track.freeze` |
| Render-in-Place | Live 12.3, Reaper† | v1 | `track.bounce_in_place` |
| Anticipative FX Processing | Reaper† | v2 | – |

### 9.15 Controller

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| MIDI-Learn für alle Parameter | alle | v1 | `control.midi_learn` |
| Mackie Control (MCU) / HUI | Ardour 9 (Mackie-Control-Erweiterungen), Reaper† | v1 | `control.add_surface` |
| OSC | Reaper†, Ardour† | v2 | – |

### 9.16 KI-Funktionen (Marktstand 2026, zur Einordnung)

| Feature | Herkunft | Prio | Claude-Tools |
|---|---|---|---|
| Agentischer Assistent, der in der DAW handelt | FL Studio 2026 Gopher; Studio Pro 8.1 Studio Assistant; Waveform 14 AI Assistant | **MVP – Kern von Tracklab** | alle |
| Stem-Separation | Cubase 15, Logic Stem Splitter, Live 12.3 (Suite), FL Studio „Remix a Song“, Studio Pro 8.1 (Moises) | später (Library/Lizenz klären) | `ai.separate_stems` |
| Audio-to-MIDI / Drum-Extraktion | Studio Pro 8 / 8.1 | später | – |
| Speech-to-Text | Pro Tools | später | – |
| Session Players | Logic | Nicht-Ziel | – |

### 9.17 Barrierefreiheit

| Feature | Herkunft | Prio |
|---|---|---|
| Vollständige Tastaturbedienung, sichtbarer Fokus | – | v1 |
| Screenreader-Unterstützung | Reaper + OSARA† | v2 |
| Skalierung, Kontrastthema, farbenblindsichere Meter | – | v1 |
| Claude-Sprachbefehle als Bedienhilfe | – | v2 |

---

## 10. Mastering & Export/Import im Detail

### 10.1 Formate

| Format | Export | Import | Hinweis |
|---|---|---|---|
| WAV/BWF (16/24/32f) | MVP | MVP | BWF-Metadaten, iXML optional |
| FLAC | MVP | MVP | Vorbis Comments |
| MP3 | MVP | MVP | Encoder-Lizenz per ADR klären (LAME, LGPL) `[VERIFIZIEREN]` |
| AIFF, Ogg Vorbis | v1 | v1 | – |
| Opus | v2 | v2 | – |
| DAWproject, MIDI | v1 | v1 | – |
| AAF, RPP | – | v2 | – |

### 10.2 Lautheits-Presets `[VERIFIZIEREN vor Release; Spotify ist durch Spotify for Artists belegt, die übrigen Werte stammen überwiegend aus Mastering-Guides]`

| Preset | Integrated | True Peak | Bemerkung |
|---|---|---|---|
| Video-Pipeline (eigenes Preset) | −14 LUFS | −1 dBTP | WAV 48 kHz/24-bit pro Song für DaVinci Resolve; entspricht dem bisherigen Lautheitsschritt der Video-Pipeline |
| Spotify | −14 LUFS | −1 dBTP (−2 dBTP bei Masters lauter als −14) | Quelle: Spotify for Artists; Premium-Stufen Loud −11 / Normal −14 / Quiet −19 LUFS |
| Apple Music | −16 LUFS | −1 dBTP | Sound Check; von Apple nicht selbst veröffentlicht, sondern gemessener Durchschnittswert |
| YouTube | −14 LUFS | −1 dBTP | nur Absenkung |
| Amazon Music | −14 LUFS | −2 dBTP | – |
| Tidal | −14 LUFS | −1 dBTP | Album-Normalisierung |
| Deezer | −15 LUFS | −1 dBTP | geht auf eine Community-Antwort von 2019 zurück; Normalisierung immer aktiv, nur pro Track |
| EBU R128 Broadcast | −23 LUFS | −1 dBTP | Referenz/Test |
| Kein Ziel – nur messen | – | – | Standard für den Master: Musik zuerst |

UI-Hinweis im Preset-Dialog: Plattformen normalisieren beim Abspielen; die Werte sind kein Upload-Zwang.

### 10.3 Render-Pipeline
Master-Kette → optionale Normalisierung (zweistufig: messen → Gain → True-Peak-Limiter) → Samplerate-Konvertierung → **Dither nur bei Bittiefen-Reduktion, immer als letzte Stufe** → Encoder → Metadaten → Prüfmessung der fertigen Datei (`analyze.loudness`), Bericht als `.json` neben der Datei.

---

## 11. Qualität, CI, Release

### 11.1 Tests zuerst
Jede Karte beginnt mit dem test-writer. Testarten: Unit (core, dsp), Engine-Integration (Offline-Render), Command-Registry-Vertrag (jedes Command: Schema gültig, Undo/Redo symmetrisch, Rückgabe-Schema), Assistant-Simulation (aufgezeichnete Tool-Calls → erwarteter Projektzustand, ohne Netz), MCP-Conformance, GUI-Screenshots.

### 11.2 Golden-File-Render-Tests
Fixture-Projekte in `tests/fixtures/` werden headless gerendert und gegen Golden-WAVs verglichen (Null-Test, z. B. Restsignal < −90 dBFS, feste Seeds):
- `mitschnitt-mini`: synthetisch erzeugter Stereo-Mitschnitt mit 3 „Songs“, Pausen und Applaus-Rauschen – für Songgrenzen-Erkennung, Render pro Region und Normalisierung.
- `band-mini`: 12 kurze Spuren, Busse, Sends.

Golden-Updates nur per Karte mit Begründung. Echte Mitschnitte bleiben lokal (R13).

### 11.3 LUFS-Validierung
Der eigene Loudness-Meter wird mit den EBU-Testsignalen **Tech 3341** (Momentary/Short-term/Integrated, True Peak) und **Tech 3342** (LRA) validiert. Ein CI-Skript lädt die Signale von der EBU-Website; sie werden nicht ins Repo committet `[VERIFIZIEREN: Lizenz]`. Toleranzen laut Spezifikation `[VERIFIZIEREN: aktuelle Versionen von ITU-R BS.1770 und EBU R128/Tech 3341/3342]`.

### 11.4 Headless-CLI `tracklab-cli`
```
tracklab-cli render <projekt> --out <datei> [--range region:<name>|regions] [--format wav24]
tracklab-cli analyze <datei|projekt> --loudness --truepeak --lra --spectrum --json
tracklab-cli find-songs <datei> [--setlist <setlist.txt>] --json
tracklab-cli run-commands <projekt> <commands.json> --save-as <out>
tracklab-cli screenshot <projekt> --view arrange|mixer|editor|master --theme dark|light --scale 1|2 --out <png>
tracklab-cli validate-lufs <ebu-testsignal-ordner>
tracklab-cli export-tools --out tools.json
```
Damit prüfst du Audio-Output und GUI selbst, ohne Audio-Hardware.

### 11.5 RealtimeSanitizer
Eigener CI-Job (Linux, Clang): Engine-Tests und Fixture-Renders mit `-fsanitize=realtime`. Audio-Callbacks sind `[[clang::nonblocking]]` annotiert. Jeder Befund blockiert das Gate.

### 11.6 CI (GitHub Actions: `gate.yml` + Packaging)

| Job | Runner | Inhalt |
|---|---|---|
| build-test-linux | ubuntu-24.04 (Basis von Mint 22.x) | GCC + Clang, Tests, Golden, LUFS, RTSan |
| build-test-windows | windows-latest | MSVC, Tests, Golden |
| package-linux | ubuntu-24.04 | AppImage + .deb |
| package-windows | windows-latest | Installer (Inno Setup oder NSIS, per ADR) + portables ZIP |
| smoke | beide | Start im Headless-Modus, `--version`, Fixture-Render |
| docs-check | ubuntu | `docs/commands.md`/`tools.json` aktuell, Links |

- Jeder Push auf `main` erzeugt Artefakte als Workflow-Download, damit ich ohne eigene Build-Umgebung testen kann; den Link trägst du in `team/RESUME.md` und in die Prüfaufgabe ein.
- Tag `vJJJJ.MM.N` → GitHub Release mit allen Artefakten und deutschem Changelog.
- Caching (ccache/sccache); JUCE und Tracktion Engine als Submodule oder per FetchContent mit festen Commits.
- Optional ab v1: Kompatibilitätsjob auf `ubuntu-26.04` (Basis von Mint 23) `[VERIFIZIEREN: Runner-Verfügbarkeit]`.

### 11.7 Release-Prozess
1. Meilenstein-DoD erfüllt → Prüfaufgabe (V) in `team/TODO-PO.md` mit Artefakt-Links, Screenshots und Testprotokoll (`docs/testing/manual/…`).
2. Ich teste unter Windows und Linux Mint mit echtem Interface und schreibe Review-Notizen.
3. Freigabe im Chat → du setzt den Tag (Cloud-Session: 403 → ich setze ihn), Release, `Projektinhalt.md` und Vault-Abschnitte nachziehen; den Verlauf schreibt der Hook. Releases bündeln.

---

## 12. Roadmap (jeder Meilenstein durchläuft den Loop §3.2)

| M | Ziel | Akzeptanz (Auszug) |
|---|---|---|
| **M0** | Vorlage übernommen, Gate auf C++ umgebaut, Agents ergänzt, Recherche-Themen, ADR-001-Vorschlag, Engine-Spike | Repo und Vault-Sync stehen; Entscheidungsrunde 1 beantwortet; Spike baut in der CI für Windows + Linux |
| **M1** | Fundament: CMake, JUCE + Tracktion, Audio-I/O (ASIO/WASAPI/ALSA/JACK), Projektformat, Autosave, Command-Registry v1, Undo, Headless-CLI (`render`, `analyze`), CI + Artefakte | Projekt anlegen/speichern/öffnen; Fixture-Render golden; Artefakte downloadbar |
| **M2** | Mitschnitt-Workflow ohne Claude: Audio-Import (WAV/FLAC/MP3), Arrange-Grundgerüst (Timeline, Transport, Theme-Tokens, Screenshot-CLI), Marker/Regionen, Split/Trim/Fades, Dynamic Split, LUFS/True Peak (EBU-validiert), Normalisierung, Render pro Region | Workflow A (§4.3) von Hand durchgespielt; Export passt in die Video-Pipeline |
| **M3** | Claude-Integration MVP: Panel (Streaming, 3 Modi, Plan-Vorschau), Tool-Generierung, strict, Tool Search, Caching, Turn-Undo, Keychain, Kosten; `analyze.find_song_boundaries`; MCP-Server (localhost, Token, opt-in) + stdio-Shim | Workflow A per Claude-Befehl; dieselben Befehle über Claude Code via MCP |
| **M4** | Spuren & Aufnahme: Track-Typen, Ordner, Busse, 12-Kanal-Aufnahme, Monitoring, Metronom, Punch, Loop-Takes, Latenzmessung | Ich nehme 12 Kanäle auf (Testprotokoll) |
| **M5** | Editing & Comping: Lanes, Swipe-Comping, Razor, Fades/Crossfades, Clip-Gain, Ripple, Snap, Gruppen-Edit | Gesang aus 3 Takes gecomped; Drum-Edit phasenkohärent |
| **M6** | Mixer & Plugins: Konsole, Inserts, Sends, Sidechain, PDC, VST3-Hosting + Scanner + Sandbox, Basis-FX, Meter | Drum-Bus + Parallelkompression per Claude-Befehl; Plugin-Crash ohne Projektverlust |
| **M7** | Automation & MIDI-Basis: Lanes, Modi, Kurven; Piano-Roll, Drum-Editor | Fader-Ride in Touch; MIDI-Spur bearbeitet |
| **M8** | Mastering & Export komplett: Mastering-Seite, Referenz-A/B, Dither, Render-Queue, Region-Matrix, Stems, Presets, Metadaten | Workflow B Ende-zu-Ende exportiert, Messbericht korrekt |
| **M9 = 1.0** | Interop & Politur: DAWproject, MIDI, LV2, CLAP, Reaper-Shortcut-Preset, Screensets, Light-Theme, Barrierefreiheit-Basis, Installer | kompletter Akzeptanztest A + B durch mich |
| danach | AAF-/RPP-Import, ARA, Spektral-Editing, Modulatoren, Mastering-Assistent, Control Room, MCU/OSC, Stem-Separation | per Entscheidung |

Optimierungsrunden nach M2, M5 und M8.

---

## 13. Erster Auftrag (Session 1) – genau in dieser Reihenfolge

1. **Vault lesen (R2):** `Tracklab/Projektinhalt.md`, `Notiz „Agent-Team-Vorlage“`, `Vault-Sync-Notiz (Git-Hook)`, `Notiz „Video-Pipeline“ (Nachbarprojekt)` (Übergabe Audio → Video) und eine Suche nach „Tracklab“. Kurzfassung nach `team/research/vault-kontext/README.md`, sobald `team/` existiert.
2. **Repo anlegen:** Vorschlag `brummilab/tracklab`, privat (wie meine anderen privaten Projekte). Per `gh`, falls angemeldet; sonst `USER INPUT REQUIRED`. Lokal `git init`, Branch `main`.
3. **Vorlage übernehmen** (§3.1, Schritte 1–5). Kein Zugriff auf `agent-team-vorlage` → `USER INPUT REQUIRED`.
4. **Grundgerüst:** `.gitignore`, `LICENSE` (Platzhalter bis zur Entscheidung), `CLAUDE.md`, `README.md` (Deutsch: Ziel, Status, wo die Builds liegen), `Projektinhalt.md` (Inhalt aus der Vault-Notiz übernehmen, ohne `## Verlauf`), `src/`, `tests/`, `docs/`, `.claude/settings.json` mit Hook `scripts/hooks/guard-git.sh`, `scripts/gate.sh`/`gate.ps1` (vorerst Doku- und Secret-Checks; CMake folgt mit dem Spike), `gate.yml`.
   **Branding übernehmen:** alle Dateien aus dem Vault-Ordner `Tracklab/branding/` (inkl. `png/`) unverändert nach `assets/branding/` kopieren – lokal von `<Vault>\Tracklab\branding\`, sonst SVGs, `README.md` und `render-icons.py` per `vault_read` holen und PNG/ICO mit `render-icons.py` neu erzeugen. Logo oben in die README (`assets/branding/png/tracklab-logo-light.png`).
5. **Vault-Sync anbinden (R6):** lokal `git config vault.note "<Vault>/Tracklab/Projektinhalt.md"`; in einer Cloud-Session stattdessen `USER INPUT REQUIRED` mit den Befehlen für post-merge-Hook und geplanten Pull (Vorbild: ein anderes eigenes Projekt).
6. **Recherche-Themen anlegen** (je Ordner unter `team/research/` mit Fragen und Quellenliste): `daw-features`, `engine-spike`, `songgrenzen-erkennung`, `plugin-hosting-clap-lv2`, `audio-backends-linux`, `claude-api`, `mcp-server` (inkl. Reaper-MCP-Referenzen), `loudness-standards`, `gui-design-system`, `lizenz-und-name`, `ci-packaging`.
7. **Entscheidungsrunde 1** in `team/TODO-PO.md` – je Frage Empfehlung und Default, Ergebnisse später als E-Einträge:
   - Repo-Ort und Sichtbarkeit: privat `brummilab/tracklab` (Empfehlung) oder öffentlich (dann vorher Markenprüfung „Tracklab“)
   - Lizenz: AGPLv3 (Empfehlung, §5)
   - Stack: ADR-001 nach §5 inkl. Alternativen
   - Reihenfolge: Workflow A zuerst (M2/M3), danach Recording und Mixing (Empfehlung, §12)
   - Standardmodell im Claude-Panel: `claude-sonnet-5-5` (Empfehlung)
   - Windows-Installer: Inno Setup oder NSIS
   - Release-Schema `vJJJJ.MM.N` wie in meinen anderen Projekten (bestätigen)
   - Agent Teams aktivieren? (Empfehlung: nein)
   - Info: Audio-Interface bzw. womit die Band heute aufnimmt (Modell, Eingänge, Treiber)
   - Info: welcher Reaper-MCP-Server heute läuft (Name/Repo) – als Designreferenz
   - Info: Reaper-Shortcuts und Custom Actions für das Preset (`reaper-kb.ini` exportieren)
   - Info: Entwicklung lokal am Laptop oder in Cloud-Sessions (wichtig für Vault-Sync und Tag-Push)
8. **Commit & Push auf `main`**, README und `Projektinhalt.md` aktuell, Vault-Abschnitte *Status* und *Offene Punkte* nachgezogen, `team/RESUME.md` = „Warte auf Entscheidungsrunde 1“.
9. **Stoppen mit `USER INPUT REQUIRED`** (Format §3.5) mit Verweis auf `team/TODO-PO.md`.
