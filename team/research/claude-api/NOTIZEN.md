# Claude-API – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead. Quellen: offizielle Doku auf platform.claude.com
(Seiten ohne eigenes Datum, abgerufen 08.10.2026), JUCE- und curl-Quellcode (GitHub master).

## 1. Tool-Namen
- **Belegt:** Muster `^[a-zA-Z0-9_-]{1,128}$`, **keine Punkte** → `track.create` wird `track_create`
  (https://platform.claude.com/docs/en/agents-and-tools/tool-use/define-tools).
- Folgerung: Registry hält eine Map `toolName → commandId` und prüft Kollisionen beim Start (z. B. `track.create_folder`
  vs. `track_create.folder`); nicht per String zurückparsen. Namensraum-Präfixe empfohlen (wichtig für Tool Search).

## 2. `strict: true`
- Top-Level-Feld der Tool-Definition, kein Beta-Header. Jedes Objekt braucht `additionalProperties: false`.
- Unterstützt: Grundtypen, `enum` (Skalare), `const`, `anyOf`, `allOf` (nicht mit `$ref`), interne `$ref`/`$defs`,
  `default`, `required`, String-Formate (`date-time`, `uuid`, `uri` …), `minItems` 0/1, `pattern` ohne Lookaround/`\b`.
- **Nicht** unterstützt: `minimum`, `maximum`, `multipleOf`, `minLength`, `maxLength`, `maxItems`, `minItems` > 1,
  rekursive/externe Schemas → HTTP 400.
- **Limits je Request:** 20 strict-Tools, 24 optionale Parameter, 16 Union-Parameter (Summe aller strict-Schemas);
  Grammatik-Cache 24 h; Änderung von Struktur/Tool-Menge kompiliert neu.
- **Folge:** „alle Tools strict“ (Auftrag §7.3) ist nicht umsetzbar → strict nur für ausgewählte Tools, Wertebereiche
  (min/max) immer lokal gegen das Registry-Schema prüfen.
- strict + `defer_loading` kombinierbar; ob die Limits für die ganze Tool-Menge gelten `[VERIFIZIEREN]` (Test mit Key).

## 3. Tool Search
- **Belegt:** `{"type":"tool_search_tool_bm25_20251119","name":"tool_search_tool_bm25"}` und
  `{"type":"tool_search_tool_regex_20251119","name":"tool_search_tool_regex"}`.
- `defer_loading: true` je Tool; alle Definitionen weiter im `tools`-Array; mindestens ein Tool non-deferred, Such-Tool
  nie deferred; 3–5 Kern-Tools non-deferred empfohlen.
- Limits: 10.000 deferred Tools, 5 Treffer Standard, Regex ≤ 200 Zeichen, BM25-Query ≤ 500 Zeichen. Serverseitig; kein
  `tool_result` auf `srvtoolu_…`; `server_tool_use`/`tool_search_tool_result` unverändert in die History. Keine Extrakosten.
- Fehler kommen als HTTP 200 mit `tool_search_tool_result_error` (`invalid_tool_input`, `unavailable`,
  `too_many_requests`, `execution_time_exceeded`).
- **Modelle:** Opus/Sonnet/**Haiku 5.5**, Fable 5.1 u. a. → Haiku-Vorbehalt in §7.5 **widerlegt**.
- Eigene Suche möglich: Custom-Tool liefert `tool_reference`-Blöcke.

## 4. Prompt Caching mit deferred Tools
- Prefix-Reihenfolge `tools` → `system` → `messages`; deferred Tools liegen nicht im Prefix, Cache bleibt erhalten.
- `cache_control` auf das letzte **non-deferred** Tool; auf einem deferred Tool → HTTP 400.
- Invalidierung: Tool-Definition ändert → alles; `tool_choice`/`thinking`/`effort` ändern → mindestens Messages.
  Tool-Liste und Reihenfolge je Session stabil halten.
- Mindestlänge 512 Tokens (5.5er-Modelle); TTL 5 min Standard, `"ttl":"1h"` möglich (1h-Breakpoints vor 5m);
  max. 4 Breakpoints, Lookback 20 Blöcke; „Automatic caching“ per Top-Level-`cache_control`.
- Empfehlung: explizite Breakpoints auf Tools + System, Automatic Caching für den Verlauf.

## 5. Effort
- `output_config.effort` (GA), Werte `low|medium|high|xhigh|max` (https://platform.claude.com/docs/en/build-with-claude/effort).
- Defaults: Opus 5.5 und Haiku 5.5 `medium`, Sonnet 5.5 `high`.
- Mitten im Chat ändern ohne Cache-Verlust: Beta-Header `mid-conversation-output-config-2026-07-01` + System-Nachricht mit
  `output_config`.
- Thinking-Kombinationen: Opus 5.5 lehnt `thinking.disabled` ab; Sonnet 5.5 nutzt `between_tools` (nur bis `high`);
  Haiku 5.5 `disabled` nur bis `high`.

## 6. Streaming und `usage`
- `message_start` → je Block `content_block_start`/`…_delta`/`…_stop` → `message_delta` → `message_stop`; dazu `ping`,
  `error` (z. B. `overloaded_error` mitten im Stream). Unbekannte Events ignorieren.
- Tool-Input: `input_json_delta.partial_json` je `index` sammeln, bei `content_block_stop` abgesichert parsen.
  `eager_input_streaming` nicht einschalten.
- `usage`: `input_tokens`, `output_tokens`, `cache_creation_input_tokens` (aufgeteilt in `ephemeral_5m/1h`),
  `cache_read_input_tokens`. `message_delta.usage` ist **kumulativ** – letzten Wert nehmen.
  Gesamt-Input = `cache_read + cache_creation + input_tokens`.

## 7. Modelle und Preise
- **Belegt:** `claude-opus-5-5`, `claude-sonnet-5-5`, `claude-haiku-5-5` (ohne Datums-Suffix, gepinnte Snapshots;
  1M Kontext, 128K Output). Neuer: `claude-fable-5-1`.
- Preise USD/MTok (https://platform.claude.com/docs/en/about-claude/pricing):

| Modell | Input | Cache Write 5m | Cache Write 1h | Cache Read | Output |
|---|---|---|---|---|---|
| claude-opus-5-5 | 4 | 5 | 8 | 0.20 | 20 |
| claude-sonnet-5-5 | 2 | 2.50 | 4 | 0.10 | 10 |
| claude-haiku-5-5 (≤ 100.000 Tokens Prompt) | 0.10 | 0.125 | 0.20 | 0.01 | 0.50 |
| claude-haiku-5-5 (> 100.000 Tokens Prompt) | 0.50 | 0.625 | 1 | 0.05 | 2.50 |
| claude-fable-5-1 (Info) | 10 | 12.50 | 20 | 0.25 | 50 |

- Tool-Overhead im System-Prompt: 286 Tokens (auto/none).
- **Neu, betrifft §7.5:** Auf Opus 5.5 und Sonnet 5.5 ist `tool_choice` `any`/`tool` → HTTP 400; nur `auto`/`none`.
  `assistant.propose_plan` lässt sich dort nicht erzwingen, nur per Prompt.

## 8. HTTP-Client für SSE
- JUCE Linux: `WebInputStream` nutzt libcurl per `dlopen` (`JUCE_USE_CURL=1`, ohne curl kein HTTPS). `read()` blockiert
  bis der Puffer voll ist; Timeout über `LOW_SPEED_LIMIT=100` B/s – riskant für SSE-Pausen (Folgerung aus Code).
- JUCE Windows: WinINet; Teil-Lese-Verhalten bei SSE `[VERIFIZIEREN]`.
- libcurl direkt: `CURLOPT_WRITEFUNCTION` liefert Daten sofort; Lizenz curl (permissiv); unter Windows über vcpkg o. Ä.
- **Empfehlung Researcher:** libcurl direkt (curl_multi im eigenen Worker-Thread) → ADR.

## Offen für den PO (→ `TODO-PO.md`)
- **F19** strict nur für ausgewählte Tools + immer lokale Schema-Validierung.
- **F20** `assistant.propose_plan` per Prompt statt `tool_choice` (5.5er-Modelle).
- **F21** HTTP-Client libcurl direkt.
- **F22** Test mit echtem API-Key (strict + deferred, Cache-Hit, SSE) – Key nur vom PO.
- F5 ergänzt: Haiku 5.5 kann Tool Search; Fable 5.1 nur auf Wunsch. `pricing.json` mit Preisstufen (Haiku).
