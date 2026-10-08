# MCP-Server

## Fragen
1. Aktuelle MCP-Spezifikation: 2025-11-25, Revision 2026-07-28? Streamable HTTP, Auth, Origin-Prüfung `[VERIFIZIEREN]`
2. Offizielle SDK-Liste (kein C++?) → eigene JSON-RPC-Implementierung vs. Rust-Shim mit `rmcp`
3. Conformance-Tests: gibt es eine offizielle Test-Suite?
4. Claude Code: `claude mcp add --transport http …` – aktuelle Syntax; Claude-Desktop-Konfiguration für stdio-Shim
5. Auswertung der Reaper-MCP-Server (DevWesC, bonfire/itsuzef, mthines, Oisub, reaper-daemon):
   Undo-Blöcke, Vorher/Nachher-Messung, lesbarer Projektzustand. Welchen nutzt David? (`TODO-PO.md` F10)

## Quellen
- https://modelcontextprotocol.io/ (Spezifikation, SDKs)
- Claude-Code-Doku (MCP)
- Repos der genannten Reaper-MCP-Server
