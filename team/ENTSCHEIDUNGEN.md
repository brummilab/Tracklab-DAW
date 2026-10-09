# Entscheidungsprotokoll

Jede beantwortete Entscheidung aus `TODO-PO.md`, neueste oben. Präfixe (E15): `E` Entscheidung (auch Vorbereitungen
durch den PO), `V` Verifikation/Prüfaufgabe durch den PO, `R` Release/Abnahme. E-Nummern folgen den F-Nummern aus
`TODO-PO.md`; nicht vergebene Nummern (E9–E12, E22) gehören zu Fragen, die noch offen sind.

| Nr | Datum | Frage | Antwort | Folge |
|---|---|---|---|---|
| E50 | 09.10.2026 | GUI-Leitbild | „Modern und aufgeräumt“, Apple-inspiriert (PO): viel Weißraum, wenige Farben (Bernstein als einziger Akzent), klare Typografie, feine Linien, dezente Schatten, keine überladenen Leisten. Standard-Theme dunkel (PO: „dunkel“), hell umschaltbar | DESIGN §5; Mockups: Artifact 8K6W6W2KNm4kxGyfjznbD8, Canva DAHXhw2g9-8 |
| E49 | 09.10.2026 | F46 – JUCE-Undo-Stash dauerhaft beheben | Default (a) (PO: „defaults ok“): kleiner Patch an JUCE (umgesetzt: Stash im neuen ActionSet von `perform()` und in `clearUndoHistory()` leeren, siehe Review O-09 C) als Patch-Datei, beim CMake-Konfigurieren idempotent angewendet; Test erkennt fehlenden Patch; Rollback danach ohne Historienverlust (F44 erledigt). Upstream-Meldung postet David (Text in `team/research/juce-undo-stash/NOTIZEN.md`) | O-09 Teil C |
| E48 | 09.10.2026 | F45 – Eigene Plugins | Default (PO: „defaults ok“): A) eigenes Plugin-Repo (JUCE, VST3+CLAP, Linux+Windows) nach M1, erstes Plugin „Lo-Fi-Kompressor“ (klanglich inspiriert von Vulf Comp, keine Namen/Logos/Oberfläche kopiert); B) eingebaute Effekte wie geplant in M6; C) DSP-Skripte erst Recherche R-01, Entscheidung vor v2 | PLAN (nach M1), R-01 |
| E47 | 09.10.2026 | Git-Historie umschreiben? | Ja (PO: „historie umschreiben“, „Force-Push erlaubt“): `git filter-repo` ersetzt in allen Commits Arbeitgeber-Name, private Vault-/Laufwerkspfade und Namen anderer Projekte; Force-Push von `main` und Arbeitsbranches am 09.10.2026. Endstand dateigleich. Alle Commit-IDs haben sich geändert | Bestehende Klone neu klonen; Verlaufszeilen im Vault zeigen auf alte IDs |
| E46 | 08.10.2026 | F43 – Vor dem Umstellen auf öffentlich | Defaults (PO: „defaults ok“): a) Vorlage darf veröffentlicht werden, Nennungen im aktuellen Stand auf „`agent-team-vorlage` (privat)“ gekürzt; b) Pfade: zunächst Default „bleiben“, dann vom PO geändert („pfade auch noch andern“) – private Vault-Pfade, lokaler Vault-Ordner und Namen anderer Projekte im aktuellen Stand neutralisiert (`<Vault>/Tracklab/…`); Historie wird nicht umgeschrieben; c) Name bleibt, Markenprüfung als Karte vor dem ersten Release; d) E44 bleibt, Release-Beine laufen wieder bei jedem Code-Push auf `main` | `gate.yml`, Auftrag R11, `Projektinhalt.md`; Karte O-07 |
| E45 | 08.10.2026 | F1 neu – Repo öffentlich? | Ja, David stellt `brummilab/Tracklab-DAW` auf öffentlich (PO: „ich werde umsteigen auf öffentliche repo“). Ersetzt E1 (privat). Vorher Prüfung durch den Lead, offene Punkte als F43 | README, `Projektinhalt.md`; Folge für CI siehe F43 d |
| E44 | 08.10.2026 | GitHub-Actions-Minuten aufgebraucht – CI sparsam? | Ja, Option 1 (PO: „1 jetzt“): CI nur bei Push auf `main` und nur, wenn sich mehr als `team/`, `docs/`, `*.md` ändert; Push = Debug-Beine (Linux GCC/Clang, Windows MSVC) + RTSan; Release-Beine nur manuell mit `full`. Lokales Gate bleibt Pflicht vor jedem Merge. Repo öffentlich machen (kostenlose Minuten) offen, entscheidet der PO | `gate.yml`; O-01/M1-09 Doppelläufe erledigt |
| E43 | 08.10.2026 | Wechsel auf Rust? (PO-Frage im Chat) | Nein – bei C++ bleiben (PO: „ok, bei C++ bleiben“) | ADR-001 bestätigt; Rust höchstens für abgegrenzte Prozesse (Plugin-Sandbox-Host, MCP-stdio-Shim) per eigener Entscheidung, frühestens beim Sandbox-Spike vor M6 |
| E42 | 08.10.2026 | F42 – Programmnamen | Default: `Tracklab` (Binary `tracklab`/`Tracklab.exe`), `tracklab-cli`, `tracklab-mcp` (PO im Chat: „Defaults ok“) | M1-Karten |
| E41 | 08.10.2026 | F41 – Autosave und Backups | Default: Autosave alle 2 min in Recovery-Datei; 10 rotierende Backups beim Speichern in `Backups/`; einstellbar (PO im Chat: „Defaults ok“) | DESIGN Rev 3 §3, M1 |
| E40 | 08.10.2026 | F40 – Endung der Projektdatei | Default: `.tracklab` (PO im Chat: „Defaults ok“) | DESIGN Rev 3 §3, M1 |
| E39 | 08.10.2026 | F39 – ADR-001 Tech-Stack bestätigen | „ADR-001 ok“ (PO im Chat) | ADR-001 angenommen: JUCE 9.0.3, Tracktion `develop` @ `bb38617`, AGPL-3.0-only, doctest; M0-05 erledigt |
| E0b | 08.10.2026 | F0b – Vault-Zugriff/Vault-Sync in Cloud-Sessions | Nicht nötig: David hat in Obsidian Git Sync aktiviert | Kein Obsidian-MCP in Cloud-Sessions; Vault-Abschnitte patcht der Lead nicht mehr selbst; `Projektinhalt.md` im Repo bleibt die gepflegte Quelle; Recherche `vault-kontext` entfällt |
| E38 | 08.10.2026 | F38 – Docking und Light-Theme | Default: Docking Eigenbau (Split/Tab-Baum, Screensets als JSON); Light-Theme mit abgedunkelten Akzenten (#B45309, #0369A1, #0F766E, #4F46E5) (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §5 |
| E37 | 08.10.2026 | F37 – Linux: Screenreader, Wayland | Default: v1 unter Linux ohne Screenreader (Tastatur, Kontrast, Skalierung ja); XWayland akzeptiert (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §5; Auftrag-Korrektur §8 |
| E36 | 08.10.2026 | F36 – Linux-Standard-Backend | Default: JACK-API über pipewire-jack, Start über `pw-jack`; ALSA Zweitoption (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §2; sudo-Schritte erst bei lokalem Test |
| E35 | 08.10.2026 | F35 – CLAP/LV2-Umfang | Default: CLAP in v1 als eigener Adapter, Planung nach Sandbox-Entscheidung; LV2 ohne X11-UI über generische Parameteransicht (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §3 |
| E34 | 08.10.2026 | F34 – Plugin-Sandbox | Default A: M6 Scanner-Isolation + Absturzerkennung; volle Sandbox als eigenes Paket nach Latenz-Spike (Karte vor M6) (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §3; Plan |
| E33 | 08.10.2026 | F33 – Lokale Test-Mitschnitte | Default: vorerst nur synthetische Fixture (PO im Chat: „Defaults ok“) | – |
| E32 | 08.10.2026 | F32 – Songgrenzen-Defaults | Default: min 60 s, max 12 min, Vorlauf 1 s, Nachklang 3 s; Ziel F ≥ 0,9 bei ±3 s (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §6 |
| E31 | 08.10.2026 | F31 – Songgrenzen-Implementierung | Default: Eigenbau ohne Fremdlibrary (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §6 |
| E30 | 08.10.2026 | F30 – Plattformen, RTSan | Default: nur x64; RTSan nur Linux/Clang ≥ 20 (Runner `ubuntu-26.04` oder `clang-20`) (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §8; M0-07 |
| E29 | 08.10.2026 | F29 – Code-Signing Windows | Default: vorerst keine Signatur; später ggf. SignPath Foundation (PO im Chat: „Defaults ok“) | – |
| E28 | 08.10.2026 | F28 – EBU-Testsignale | Default: eigene Signale nach Tech-3341-Beschreibung, bis die Terms geklärt sind; Domain-Freigabe/PDFs weiterhin erwünscht (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §8 |
| E27 | 08.10.2026 | F27 – Lautheitsmessung | Default: Engine-Messung nutzen, im Spike gegen analytische Signale, in M2 gegen EBU-Signale validieren (PO im Chat: „Defaults ok“) | M0-06 |
| E26 | 08.10.2026 | F26 – Tracktion-Stand | Default A: fester `develop`-Commit (3.5.0), Upgrade nur per Karte; Spike prüft v3.2.0 als Rückfall (PO im Chat: „Defaults ok“) | ADR-001, M0-06 |
| E25 | 08.10.2026 | F25 – Marke „Tracklab“ | Default: privat weiter; Registerrecherche vor jeder Veröffentlichung (PO im Chat: „Defaults ok“) | – |
| E24 | 08.10.2026 | F24 – JUCE 8 oder 9 | Default: Spike prüft JUCE 9.0.3 mit Tracktion `develop`, sonst 8.0.15; Ergebnis in ADR-001 (PO im Chat: „Defaults ok“) | ADR-001, M0-06 |
| E23 | 08.10.2026 | F23 – Quelltext-Angebot in der App | Default: „Über Tracklab“ mit Lizenz und Quelltext-Link, Akzeptanzkriterium M1 (PO im Chat: „Defaults ok“) | Plan M1 |
| E21 | 08.10.2026 | F21 – HTTP-Client Streaming | Default: libcurl direkt (eigener Worker-Thread) (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §2 |
| E20 | 08.10.2026 | F20 – Planvorschlag erzwingen | Default: über System-Prompt, `tool_choice: auto` (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4 |
| E19 | 08.10.2026 | F19 – `strict` für Tools | Default: strict nur für Kern-Tools + destruktive Commands, immer lokale Schema-Validierung (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4 |
| E18 | 08.10.2026 | F18 – MCP-Auth | Default: statisches Bearer-Token (Umgebung/Schlüsselbund), Origin- + Host-Prüfung, nur 127.0.0.1, 403 (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4; ADR später |
| E17 | 08.10.2026 | F17 – MCP-Implementierung | Default A: eigene C++-JSON-RPC-Implementierung + C++-stdio-Shim, Conformance-Suite im Gate (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4 |
| E16 | 08.10.2026 | F16 – MCP-Spec-Version | Default: 2026-07-28 primär, 2025-11-25 Fallback (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4 |
| E15 | 08.10.2026 | F15 – Präfix `V` | Default A: Auftrag gilt – `V` = Verifikation durch den PO, Vorbereitungen als `E`, `R` = Release (PO im Chat: „Defaults ok“) | `team/README.md`, Kopf dieses Protokolls |
| E14 | 08.10.2026 | F14 – Hook ↔ Vorlage (`git merge`) | Default A: Merge/Commit in Worktrees nur blocken, solange `HEAD` auf `main`; Push und `checkout main` immer blocken (PO im Chat: „Defaults ok“) | bereits umgesetzt (M0-03) |
| E13 | 08.10.2026 | F13 – Push-Weg Cloud | Default: Arbeitsbranch und `main` pushen, `main` führend (PO im Chat: „Defaults ok“) | – |
| E8 | 08.10.2026 | F8 – Agent Teams (experimentell) | Default: nein (PO im Chat: „Defaults ok“) | – |
| E7 | 08.10.2026 | F7 – Release-Schema | Default A: Tag `vJJJJ.MM.N` nach Freigabe im Chat; Push auf `main` prüft nur; strikte Regex-Prüfung im Release-Workflow (PO im Chat: „Defaults ok“) | DESIGN §8 |
| E6 | 08.10.2026 | F6 – Windows-Installer | Default: Inno Setup 6.7.x (auf `windows-2025` vorinstalliert) (PO im Chat: „Defaults ok“) | DESIGN §8 |
| E5 | 08.10.2026 | F5 – Standardmodell Claude-Panel | Default: `claude-sonnet-5-5`; Opus für Analyse/Planung; Haiku 5.5 (kann Tool Search); Fable nur auf Wunsch (PO im Chat: „Defaults ok“) | DESIGN Rev 2 §4 |
| E4 | 08.10.2026 | F4 – Reihenfolge | Default: Workflow A zuerst (M1 → M2 → M3), dann Recording/Mixing (PO im Chat: „Defaults ok“) | Plan |
| E3 | 08.10.2026 | F3 – Tech-Stack | Default: Vorschlag angenommen unter Vorbehalt des Engine-Spikes; ADR-001 nach Spike zur Bestätigung (PO im Chat: „Defaults ok“) | ADR-001 (Entwurf), M0-06 |
| E2 | 08.10.2026 | F2 – Lizenz | Default: AGPLv3 (SPDX `AGPL-3.0-only`) (PO im Chat: „Defaults ok“) | `LICENSE` ersetzt |
| E1 | 08.10.2026 | F1 – Repo-Ort/Sichtbarkeit | Default: `brummilab/Tracklab-DAW`, privat (PO im Chat: „Defaults ok“) | – |
| E0 | 08.10.2026 | Arbeitsweise mit Agent-Team einführen? | Ja (Auftrag §3) | M0: `team/`, `.claude/agents/`, Gate; Vorlage v1.0.0 vom PO als ZIP bereitgestellt und übernommen (M0-03) |
