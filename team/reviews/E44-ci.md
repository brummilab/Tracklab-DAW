# Review CI-Sparmodus (E44, ohne Karte)

Datum: 08.10.2026 · Reviewer: `reviewer` · Urteil: **OK** (1 Runde)

- `fromJSON(inputs.full && … || '["debug"]')`: Push → Debug, Dispatch `full` → Debug + Release. Geprüft.
- mp3-compare: Referenz gcc-debug, bricht bei < 2 Dateien oder Abweichung ab (nachgestellt mit `bash -eo pipefail`).
- Nichts hängt an Release-Beinen (tidy auf clang-debug, Artefakt-/Cache-Namen nach `matrix.config`).
- `paths-ignore` nimmt reinen Doku-/Akten-Pushes die CI-Prüfung (Pflichtdateien, Secret-/Audio-Scan) – vertretbar, weil
  lokales Gate Pflicht ist.

Hinweise und Umsetzung durch den Lead:
1. Lead-Pushes ohne CI-Scan → Regel in `team/README.md`: vor jedem Push `gate.sh static`. ✅
2. DESIGN §8 auf E44 verwiesen. ✅
3. Herkunft der PO-Testbuilds (Debug/Release) → in M1-09 aufgenommen. ✅
4. M1-09-Satz bereinigt. ✅
5. Concurrency-Kommentar in `gate.yml` angepasst. ✅
6. Vault: über Obsidian Git Sync (F0b), keine Aktion.
