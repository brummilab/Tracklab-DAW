# Claude-API

## Fragen (`[VERIFIZIEREN]` aus Auftrag §7.3–§7.5)
1. Erlaubtes Muster für Tool-Namen (Punkte erlaubt? → `track_create`)
2. `strict: true` für Tools: Anforderungen an JSON-Schema
3. Tool Search: aktueller Typname (`tool_search_tool_bm25_*` / `_regex_*`), `defer_loading`, Limits, Modellunterstützung (Haiku?)
4. Prompt Caching mit deferred Tools: wo `cache_control` setzen
5. Effort-Parameter: Name, Werte, Modelle
6. SSE-Streaming-Events, `usage`-Felder inkl. Cache-Read/-Creation; Preise für `presets/pricing.json`
7. Aktuelle Modell-IDs: `claude-opus-5-5`, `claude-sonnet-5-5`, `claude-haiku-5-5`
8. HTTP-Client: libcurl vs. JUCE `WebInputStream` für SSE unter Windows und Linux

## Quellen
- Offizielle API-Doku von Anthropic (Messages, Tool Use, Tool Search, Prompt Caching, Streaming, Models, Pricing)
