# JUCE UndoManager – veralteter Redo-Stash (O-09 Teil A)

Stand: 09.10.2026 · Lauf: `researcher` · Übernommen vom Lead.

## Befunde
- **Nicht behoben upstream:** `develop` 3ac433b3fd (08.10.2026) und `master`/Tag 9.0.3 (be29c81) – `undomanager/`
  identisch. Stash-Mechanismus seit Commit bc17cb90a8 (08.08.2016) gewollt; Lücke: alter Stash wird bei `perform()` mit
  leerem Redo-Stack nicht ersetzt, `clearUndoHistory()` leert ihn nicht. Bekannte Meldung nicht gefunden (Forum-Thread
  47462 nur per Suchzusammenfassung; GitHub-Issue-Suche gesperrt) `[VERIFIZIEREN]`.
- **Tracktion:** produktiv nur `tracktion_CurveEditor.cpp:512` (Echtzeit-Drag) – betroffen, falls Tracklab diesen
  Editor nutzt `[VERIFIZIEREN]`. Tracklab: nur `src/core/transaction.cpp` (`rollback`).
- **Minimaler Fix – korrigiert in O-09 C:** Stash im `else`-Zweig von `perform()` (neuer ActionSet) und in
  `clearUndoHistory()` leeren (5 Zeilen). Die ursprünglich notierte Variante unten wäre falsch (Review O-09 C): `stashedFutureTransactions.clear()` in `moveFutureTransactionsToStash()` außerhalb des
  `if`; zusätzlich in `clearUndoHistory()`.
- **Ohne JUCE-Änderung:** kein sauberer Weg (Feld privat, keine virtuellen Methoden; Dummy-Action-Trick erzeugt
  sichtbaren Geister-Eintrag).

## Optionen
| Option | Aufwand | Pflege | Risiko |
|---|---|---|---|
| a) Patch-Datei, idempotent beim CMake-Konfigurieren | klein | gering, bei Pin-Update prüfen | Submodul „dirty“; Test erkennt fehlenden Patch |
| b) Fork brummilab/JUCE | mittel | hoch für 2 Zeilen | Abhängigkeit eigenes Repo |
| c) Upstream-Fix abwarten | null | – | Zeitpunkt offen |
| d) Zwischenlösung (Historie verwerfen) | – | – | Historienverlust (F44) |

**Empfehlung:** a) + Upstream-Meldung; Teil B (Vorabprüfung) zusätzlich.
Lizenz: Änderung an JUCE unter AGPL zulässig, Quelltext ist öffentlich (keine Rechtsberatung) `[VERIFIZIEREN]` bei
späterer kommerzieller JUCE-Lizenz.

## Upstream-Issue (Entwurf, Englisch – David postet, Kanal Forum/GitHub `[VERIFIZIEREN]`)
```
Title: UndoManager: stale stashed redo transactions are restored by undoCurrentTransactionOnly()

JUCE version: 9.0.3 (also develop @ 3ac433b3fd, 2026-10-08, same code)

Summary
UndoManager::undoCurrentTransactionOnly() restores a "stash" of former redo transactions (introduced in
bc17cb90a8). The stash is only replaced in moveFutureTransactionsToStash() when redo transactions exist at the
time of perform(). If the redo stack is empty, a stash from an earlier cycle survives and is appended to the redo
stack by undoCurrentTransactionOnly(). Redo then re-applies a change that no longer matches the current state.
clearUndoHistory() does not clear the stash either.

Minimal example
  UndoManager um;   // A, B, C, D are trivial UndoableActions changing a shared int
  um.perform (new A());  um.beginNewTransaction();
  um.perform (new B());  um.undo();            // redo stack: [B]
  um.beginNewTransaction();
  um.perform (new C());                        // B moves to the stash, redo stack empty
  um.beginNewTransaction();
  um.perform (new D());                        // redo stack empty -> stash NOT replaced
  um.undoCurrentTransactionOnly();             // undoes D, re-appends stale stash [B]
  Expected: um.canRedo() == false.   Actual: canRedo() == true; redo() re-applies B after C.
Same with clearUndoHistory() followed by perform() and undoCurrentTransactionOnly().

Suggested fix
Clear stashedFutureTransactions where perform() starts a new ActionSet (the else branch) and in clearUndoHistory().
Not unconditionally in moveFutureTransactionsToStash(): that runs after every perform(), so the second write of a
transaction that began with redo steps would drop the stash it just filled, and undoCurrentTransactionOnly() could
no longer restore them.
```
Minimalbeispiel vor dem Posten als Test reproduzieren (Teil C).
