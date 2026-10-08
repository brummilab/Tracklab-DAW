# TODO für David (Product Owner)

Nur **Entscheidungen** und **Reviews/Prüfungen**. Antwort direkt unter die Frage schreiben (`**Antwort:** …`) oder
im Chat. Der Lead überträgt sie nach `ENTSCHEIDUNGEN.md` und streicht den Punkt.

F0 (Vorlage) ist erledigt: v1.0.0 per ZIP übernommen (E0).

---

## Blocker (08.10.2026)

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
- **Recherche 08.10.2026:** Inno Setup 6.7.1 ist auf dem Runner `windows-2025` vorinstalliert (`team/research/ci-packaging/NOTIZEN.md`).
- **Antwort:**

### F7 – Release-Schema und wann ein Stand „live“ ist (deckt Vorlagen-Frage E1 ab)
- **A (Empfehlung):** Release per Tag `vJJJJ.MM.N` wie in meinen anderen Projekten, Releases gebündelt; Push auf `main` prüft nur
  (Gate) und erzeugt Test-Builds. Tag erst nach ausdrücklicher Freigabe im Chat (Cloud: du setzt den Tag).
- B: jeder Push auf `main` gilt als Release.
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
- **Hinweis:** In dieser Cloud-Session fehlt der Obsidian-MCP (F0b).
- **Antwort:**

### F13 – Push-Weg in Cloud-Sessions
Die Cloud-Session ist auf den Arbeitsbranch `claude/new-session-mymk20` konfiguriert; laut R3 pushe ich
zusätzlich direkt auf `main` (Fast-Forward, kein PR).
- **Empfehlung:** so beibehalten.
- **Default:** beide pushen, `main` ist führend.
- **Antwort:**

### F14 – Widerspruch Hook ↔ Vorlage: `git merge` in Worktrees
Auftrag §3.6: der Hook blockt `git merge` in Worktree-Sitzungen. Vorlage: der Implementer holt die Tests per
`git merge --ff-only <test-branch>` in seinem Worktree – ein pauschales Verbot würde die Schleife brechen.
- **A (Empfehlung, umgesetzt als Default):** Merge/Commit in Worktrees nur blocken, solange `HEAD` auf `main` steht;
  `git push` und `git checkout main` immer blocken. `main` bleibt geschützt.
- B: `git merge` pauschal blocken; der Implementer cherry-pickt die Test-Commits stattdessen.
- **Antwort:**

### F15 – Widerspruch Nummernpräfix `V`
Vorlage: `V` = Vorbereitung durch den PO (Zugang, Einrichtung), `R` = Release/Abnahme. Auftrag: `V` = Verifikation
durch dich (Prüfaufgabe).
- **A (Empfehlung):** Auftrag gilt (Produktprozess wie in meinen anderen Projekten): `V` = Verifikation; Vorbereitungen als `E` mit Folge
  „PO richtet ein“; `R` = Release wie in der Vorlage.
- B: Vorlage gilt; Verifikationen werden `R`.
- **Antwort:**

---

## Entscheidungsrunde 2 (aus Recherche M0-04, 08.10.2026) – F16–F33

Details jeweils in `team/research/<thema>/NOTIZEN.md`.

### F16 – MCP: Spec-Zielversion (`mcp-server`)
Seit 28.07.2026 gibt es MCP-Revision **2026-07-28** (zustandslos, kein `initialize`, keine Session-ID).
- **Empfehlung:** 2026-07-28 primär, 2025-11-25 als Fallback, bis die Unterstützung in Claude Code/Desktop belegt ist.
- **Default:** wie empfohlen.
- **Antwort:**

### F17 – MCP: Implementierungsweg (`mcp-server`)
Es gibt kein offizielles C++-SDK.
- **A (Empfehlung):** eigene JSON-RPC-Implementierung im C++-Modul `mcp` + kleiner C++-stdio-Shim für Claude Desktop;
  offizielle Conformance-Suite (`@modelcontextprotocol/conformance`, braucht Node in der CI) im Gate.
- B: Rust-Shim mit offiziellem SDK `rmcp` (zweite Toolchain).
- **Default:** A.
- **Antwort:**

### F18 – MCP: statisches Bearer-Token statt OAuth (`mcp-server`)
Spec: HTTP-Server SOLLEN OAuth 2.1 nutzen (keine Pflicht).
- **Empfehlung:** zufälliges Token (nur Umgebungsvariable/Schlüsselbund, nie im Repo), dazu Origin- **und** Host-Prüfung,
  Bindung nur an 127.0.0.1, 403 bei Fehlern; als ADR dokumentieren.
- **Default:** wie empfohlen.
- **Antwort:**

### F19 – Claude-API: `strict` nur für ausgewählte Tools (`claude-api`)
Die API erlaubt höchstens **20 strict-Tools** und 24 optionale Parameter je Request und kennt kein `minimum`/`maximum`.
„Alle Tools strict“ (Auftrag §7.3) geht deshalb nicht.
- **Empfehlung:** strict für Kern-Tools und destruktive Commands, **immer** zusätzlich lokale Validierung gegen das
  Registry-Schema (inkl. Wertebereiche). Auftrag §7.3 anpassen.
- **Default:** wie empfohlen.
- **Antwort:**

### F20 – Claude-API: Planvorschlag nicht erzwingbar (`claude-api`)
Auf Opus 5.5 und Sonnet 5.5 liefert `tool_choice` `any`/`tool` HTTP 400.
- **Empfehlung:** `assistant.propose_plan` über den System-Prompt anfordern (`tool_choice: auto`), kein Modellwechsel.
- **Default:** wie empfohlen.
- **Antwort:**

### F21 – HTTP-Client für Streaming (`claude-api`)
`juce::WebInputStream` blockiert unter Linux, bis der Lesepuffer voll ist, und bricht bei < 100 B/s ab – ungünstig für SSE.
- **Empfehlung:** libcurl direkt (eigener Worker-Thread); unter Windows über vcpkg/FetchContent. ADR.
- **Default:** wie empfohlen.
- **Antwort:**

### F22 – Test mit echtem API-Key
Einige Punkte (strict + deferred Tools, Cache-Treffer, SSE-Verhalten) lassen sich nur mit einem Key prüfen.
- **Bitte:** später einen Test-Key mit Ausgabenlimit bereitstellen – **nicht ins Repo**, nur als Umgebungsvariable
  bzw. GitHub-Secret. Wann, entscheidest du; ohne Key bleiben die Punkte `[VERIFIZIEREN]`.
- **Antwort:**

### F23 – Quelltext-Angebot in der App (`lizenz-und-name`)
Builds an Bandmitglieder sind „Weitergabe“ im Sinne der AGPL: Quelltext muss mitgehen.
- **Empfehlung:** Menüpunkt „Über Tracklab“ mit Lizenz und Quelltext-Link, Akzeptanzkriterium in M1; SPDX `AGPL-3.0-only`.
- **Default:** wie empfohlen.
- **Antwort:**

### F24 – JUCE 8 oder JUCE 9 (`lizenz-und-name`, `engine-spike`)
JUCE 9 ist erschienen (9.0.3 seit 28.09.2026), der Auftrag §5 sagt noch „nicht erschienen“. Tracktion Engine pinnt JUCE 8.0.14+.
- **Empfehlung:** im Engine-Spike die von Tracktion getestete JUCE-Version nehmen, Ergebnis in ADR-001.
- **Default:** wie empfohlen.
- **Antwort:**

### F25 – Marke „Tracklab“ (`lizenz-und-name`)
Direkter Namenstreffer: **2Simple „Tracklab“** (Mehrspur-Musikwerkzeug in Purple Mash, Schulplattform, 2026).
Registerabfragen waren aus der Cloud nicht möglich.
- **Empfehlung:** privat weiter „Tracklab“; vor jeder Veröffentlichung Registerrecherche (EUIPO/TMview, DPMA, USPTO,
  Klassen 9/41/42) durch dich, Plan-B-Namen bereithalten.
- **Default:** privat weiter, Prüfung vor Veröffentlichung.
- **Antwort:**

### F26 – Tracktion-Engine-Stand (`engine-spike`)
Letzter Release-Tag ist `v3.2.0` (Mai 2025). Der Entwicklungszweig `develop` steht auf **3.5.0 ohne Tag** und bringt genau
das, was Workflow A braucht: Mehrkanal beliebig, Render-Queue, LUFS-Normalisierung, Lautheitsmesser (R128, LRA, True Peak).
- **A (Empfehlung):** festen `develop`-Commit pinnen (`bb38617` oder neuer), Upgrade nur per Karte.
- B: Tag `v3.2.0` (stabil, aber ohne die v3.5-Funktionen).
- **Default:** A, Spike prüft parallel, ob B reicht.
- **Antwort:**

### F27 – Lautheitsmessung: Engine oder eigene Implementierung (`engine-spike`)
- **Empfehlung:** im Spike die Engine-Messung gegen analytisch bekannte Signale prüfen, in M2 gegen die EBU-Testsignale
  validieren; nur bei Abweichung eigene Implementierung.
- **Default:** wie empfohlen.
- **Antwort:**

### F28 – EBU-Dokumente und Testsignale (`loudness-standards`)
tech.ebu.ch und itu.int sind in Cloud-Sessions gesperrt; die Lizenz der EBU-Testsignale ist unbekannt.
- **Bitte (eins davon):** (a) die Domains `tech.ebu.ch`, `itu.int` im Netzwerk der Cloud-Umgebung freigeben, oder
  (b) `tech3341.pdf`, `tech3342.pdf`, `r128v5_0.pdf` und die Nutzungsbedingungen des „EBU Loudness Test Set“ lokal
  ablegen und mir den Ort nennen (nicht ins Repo).
- **Empfehlung Testsignale:** CI lädt das Set herunter und prüft SHA-256 (falls die Terms es erlauben); Rückfall: eigene
  Signale nach Tech-3341-Beschreibung erzeugen.
- **Default:** Rückfall (eigene Signale), bis die Terms geklärt sind.
- **Antwort:**

### F29 – Code-Signing Windows (`ci-packaging`)
Ohne Signatur warnt Windows SmartScreen beim Installer.
- **Empfehlung:** vorerst nicht signieren (nur Band-Builds); nach F2 und falls das Repo öffentlich wird: SignPath
  Foundation (kostenlos für Open Source). Gekaufte Zertifikate kosten ca. 80–1.500 USD/Jahr; Microsoft Artifact Signing
  geht in Österreich nur für Organisationen.
- **Default:** vorerst keine Signatur.
- **Antwort:**

### F30 – Plattformumfang und RTSan (`ci-packaging`)
- **Empfehlung:** nur x64 (kein ARM); RealtimeSanitizer nur unter Linux (Clang ≥ 20), Windows nur MSVC-Build; als
  RTSan-Basis schon in M0 den Runner `ubuntu-26.04` nutzen.
- **Default:** wie empfohlen.
- **Antwort:**

### F31 – Songgrenzen: Eigenbau statt Fremdlibrary (`songgrenzen-erkennung`)
- **Empfehlung:** Eigenbau (~500–800 Zeilen, nur JUCE-FFT: Pegel, Applaus-Erkennung, Novelty, Auswahl mit bekannter
  Songanzahl). Essentia/aubio bringen Abhängigkeiten bzw. Zusatzlizenzen; madmom-Modelle sind nicht-kommerziell.
- **Default:** Eigenbau.
- **Antwort:**

### F32 – Songgrenzen: Defaults und Zielwert (`songgrenzen-erkennung`)
Vorschlag: Mindestlänge 60 s, Höchstlänge 12 min, 1 s Vorlauf und 3 s Nachklang je Song; Akzeptanz: Grenzen-Trefferquote
(F-Measure) ≥ 0,9 bei ±3 s auf echten Mitschnitten mit Setlist.
- **Info bitte:** Wie lang ist euer kürzester und längster Song (live)? Gibt es Medleys/nahtlose Übergänge?
- **Default:** wie vorgeschlagen.
- **Antwort:**

### F33 – Songgrenzen: lokale Test-Mitschnitte (`songgrenzen-erkennung`)
Schwellen lassen sich nur an echten Aufnahmen realistisch einstellen. Echte Mitschnitte kommen **nie** ins Repo (R13).
- **Bitte (später, ab M2/M3):** 1–2 Mitschnitte lokal ablegen (Ordner außerhalb des Repos) mit einer CSV der Songgrenzen
  (Start, Ende, Titel). Tests laufen dann nur lokal, nicht in der CI.
- **Default:** nur synthetische Fixture.
- **Antwort:**

---

## Prüfaufgaben (V)
– noch keine
