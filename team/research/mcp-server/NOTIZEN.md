# MCP-Server – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.

Einschränkung: `modelcontextprotocol.io` war per Egress-Proxy gesperrt; Spec-Belege stammen aus Such-Snippets der
offiziellen Seiten, dem offiziellen Blog, GitHub und der Claude-Code-Doku. Restpunkte sind mit `[VERIFIZIEREN]` markiert.

## 1. Spezifikation

- **Belegt:** Neueste Revision ist **2026-07-28** (Vorgänger 2025-11-25).
  Quellen: https://modelcontextprotocol.io/specification/2026-07-28/changelog, https://blog.modelcontextprotocol.io/posts/2026-07-28/
- Wesentliche Änderungen 2026-07-28 (Blog):
  - kein `initialize`/`initialized`, kein `Mcp-Session-Id` – zustandslos; Version, Client-Identität und Capabilities je Anfrage in `_meta`; optional `server/discover`
  - Streamable-HTTP-Requests tragen die Header `Mcp-Method` und `Mcp-Name`
  - Benachrichtigungen über einen `subscriptions/listen`-Stream; SSE-Resumability (`Last-Event-ID`) entfällt
  - alter HTTP+SSE-Transport deprecated (12 Monate Übergang)
  - `elicitation`/`sampling`/`roots` → Multi-Round-Trip (`input_required`/`inputResponses`); Roots, Sampling, Logging deprecated
  - Listen mit `ttlMs`/`cacheScope`, deterministische Reihenfolge; Tasks als Extension `io.modelcontextprotocol/tasks`
  - Auth: `iss`-Validierung (RFC 9207), Client ID Metadata Documents statt Dynamic Client Registration
- **Origin/DNS-Rebinding:** Spec 2025-03-26 (Transports): Server MÜSSEN `Origin` prüfen, lokal SOLLEN sie nur an
  `127.0.0.1` binden. Seit 2025-11-25 laut Issues: ungültiger `Origin` → HTTP 403. Wortlaut 2026-07-28 `[VERIFIZIEREN]`.
- **Auth lokal:** Authorization ist OPTIONAL; HTTP SOLL OAuth 2.1 folgen, stdio nicht. Statisches Bearer-Token ist eine
  zulässige, per ADR zu dokumentierende SHOULD-Abweichung.

## 2. SDKs und Implementierungsweg

- **Belegt:** kein offizielles C++-SDK. Tier 1: TypeScript, Python, C#, Go; Tier 2: Java, Rust; Tier 3: Swift, Ruby,
  PHP, Kotlin (https://modelcontextprotocol.io/docs/sdk). Rust angeblich 08/2026 Tier 1 `[VERIFIZIEREN]`.
- Tier-Definition: Tier 1 = 100 % Conformance, Tier 2 = 80 % (https://modelcontextprotocol.io/community/sdk-tiers).
- C++-SDK-Diskussion ohne Zusage, am 25.09.2026 geschlossen
  (https://github.com/modelcontextprotocol/modelcontextprotocol/discussions/1271). Community-C++-Projekte ungeprüft.
- `rmcp` (offizielles Rust-SDK) 3.1.0, spricht 2026-07-28 und 2025-11-25; Lizenz und Host/Origin-Schutz `[VERIFIZIEREN]`.
- Optionen:
  - **A (Empfehlung Researcher):** eigene JSON-RPC-Implementierung im C++-Modul `mcp` + kleiner C++-stdio-Shim. Ein
    Prozess, direkter Registry-Zugriff, keine zweite Toolchain; Konformität über Conformance-Suite.
  - B: Rust-Shim mit `rmcp` – zusätzliche Toolchain, löst den IPC-Teil nicht.
  - C: nur stdio – kein Netz-Listener, aber lokale IPC zu Tracklab trotzdem nötig.

## 3. Conformance und Inspector

- **Belegt:** `@modelcontextprotocol/conformance` (https://github.com/modelcontextprotocol/conformance):
  `npx @modelcontextprotocol/conformance server --url http://localhost:<port>/mcp` (`--scenario`, `--suite`,
  `--requirements 2026-07-28`, Expected-Failures-Baseline, GitHub Action). Status „work in progress“; 2026-07-28-Szenarien
  evtl. nur in 0.2-Alpha `[VERIFIZIEREN]`. Braucht Node im Gate/CI, Version pinnen.
- MCP Inspector: `npx @modelcontextprotocol/inspector` – Debug-Werkzeug, kein Gate-Test. Version `[VERIFIZIEREN]`.

## 4. Claude Code / Claude Desktop

- **Belegt** (https://code.claude.com/docs/en/mcp): `claude mcp add --transport http tracklab http://127.0.0.1:<port>/mcp
  --header "Authorization: Bearer <token>"`; Scopes `local|project|user`; `.mcp.json` mit `${TRACKLAB_MCP_TOKEN}`-Expansion
  in `url`/`headers`; stdio: `claude mcp add <name> -- <command> [args…]`. Syntax im Auftrag §7.7 bestätigt. Token nie
  ins eingecheckte `.mcp.json`.
- Claude Desktop (stdio-Shim): `claude_desktop_config.json` unter `%APPDATA%\Claude\`, Eintrag mit `command`, `args`,
  `env`; absolute Pfade, Neustart nötig `[VERIFIZIEREN]` gegen offizielle Doku. Linux-Support von Claude Desktop nicht
  belegt → unter Linux Claude Code verwenden `[VERIFIZIEREN]`.

## 5. Reaper-MCP-Server (Designreferenz)

| Name im Auftrag | Repo | Befund |
|---|---|---|
| bonfire/itsuzef | https://github.com/itsuzef/reaper-mcp | Python/reapy, ~60 Tools, MIT; feingranular + Analyse-Tools; kein Undo, kein Vorher/Nachher |
| mthines | https://github.com/mthines/reaper-mcp | TS + Lua-Datei-Bridge, 80 Tools, MIT; Batch-Tools, Snapshots für A/B, Metering inkl. LUFS |
| Oisub | https://github.com/Oisub/reaper-mcp | 6 Tools inkl. `eval_lua`; **ein Undo-Block je Request**; `project_summary` als kompakter Zustand |
| reaper-daemon | https://github.com/wretcher207/reaper-daemon | Datei-Bridge, MIT; Undo-Block je Änderung; **`verify_change`** misst vorher/nachher; Audio-Write nur mit Freigabe |
| DevWesC | nicht gefunden | Repo-Link von David nötig (F10) |

**Übernehmen:** Undo-Transaktion je `tools/call` (bzw. Turn); Vorher/Nachher-Messung über Offline-Render (`analyze.*`);
kompakter Zustand als Tool `project.get_state` + Resource `tracklab://project/state`; Registry 1:1 als Tools, kurze
Beschreibungen, Batch-Varianten; Schreibzugriffe nur mit Freigabe.
**Nicht übernehmen:** `eval_lua`-Äquivalent, Datei-Bridge mit Polling, Rendern ohne Freigabe.

## Offen für den PO (→ `TODO-PO.md`)

- F10 (Reaper-MCP-Server) bleibt offen.
- **F16** Spec-Zielversion (Empfehlung: 2026-07-28 primär, 2025-11-25 als Fallback).
- **F17** Implementierungsweg (Empfehlung: A, eigene C++-Implementierung + C++-stdio-Shim, Conformance-Suite im Gate).
- **F18** Auth-Abweichung: statisches Bearer-Token statt OAuth, plus Origin- und Host-Prüfung (Empfehlung: ja, per ADR).
- Designfragen für den Lead (DESIGN Rev 2): Undo-Gruppierung externer `tools/call`, Claude Desktop nur Windows dokumentieren.
