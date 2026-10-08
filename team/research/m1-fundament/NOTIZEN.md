# M1-Fundament – Notizen

Stand: 08.10.2026 · Lauf: `researcher` (Quelltext Tracktion `develop` @ `bb38617`, JUCE 9.0.3; nichts gebaut) · Übernommen
vom Lead. `TE/` = `spike/engine/third_party/tracktion_engine/modules/tracktion_engine/`, `JUCE/` = `…/JUCE/modules/`.

## 1. Edit als Projektmodell
- `Edit::createEdit(Edit::Options{…})` direkt nutzen (nicht `createEmptyEdit`: Defaults 1 Spur, Master −3 dB).
  `editFileRetriever` hängt den Edit an eine Datei – **ohne `Project`/`ProjectManager`**.
- Format: ValueTree → XML (`EditFileOperations::writeToFile`); Laden XML, dann Binär; Legacy über `updateLegacyEdit`.
  Unlesbare, nicht leere Datei → `createEdit` liefert `nullptr` (Erkennungspunkt „beschädigt → Recovery“).
- **Keine Tracktion-Formatversion** → eigene Property `tracklabFormatVersion` am `EDIT`-Knoten, Migration auf dem
  ValueTree vor `createEdit`. Erhalt fremder Properties `[VERIFIZIEREN]` per Rundlauftest.
- **Datenschutz:** `modifiedBy` = `PropertyStorage::getUserName()` (Default: voller Systemname) → überschreiben.
- Relative Pfade: `SourceFileReference::setToFile(file, alwaysRelative, false)` + Edit-Property `alwaysUseRelativePaths`;
  Auflösung über `Edit::filePathResolver` (Default: Geschwister der Edit-Datei). Windows: anderes Laufwerk → absolut.
- Folder-Based Projects (3.5) passen nicht (globale Projektliste, rekursiver Scan erfasst `Backups/`/`Renders/`,
  feste Ordnernamen, Pfad-Hash-ID) → **umgehen**.

## 2. Undo
- `Edit::getUndoManager()` (juce::UndoManager), Default **30** Stufen → `Options.numUndoLevelsToStore` anheben.
- `beginNewTransaction(name)` + `Edit::UndoTransactionInhibitor` (RAII) gegen den 350-ms-Timer, der Transaktionen
  sonst zerschneidet. Muster: Inhibitor → `beginNewTransaction("…")` → Commands → Inhibitor freigeben.
- Symmetrie-Test: Zustand sichern, Command, `undo()`, Vergleich mit Normalisierer (flüchtige Properties).
  `ValueTree`-Änderungen mit `nullptr` als UndoManager sind nicht undobar → Registry muss das verhindern/prüfen.
- Strukturänderungen: `edit.dispatchPendingUpdatesSynchronously()` in Tests/CLI.

## 3. Speichern, Autosave, Recovery
- Tracktions `save()` ist **nicht atomar** (`moveFileTo` löscht Ziel zuerst), kein Autosave, keine Backups, keine Recovery.
- Atomisch: `TemporaryFile(target)` → schreiben → `flush()` (fsync/FlushFileBuffers) → `overwriteTargetFileWithTemporary()`
  (Linux `rename`, Windows `ReplaceFileW`). Vorher `edit.flushState()`, danach `resetChangedStatus()`;
  `isSaveInhibited()` respektieren. → **eigenes Speichermodul**.

## 4. JSON-Schema
| Bibliothek | Version | Lizenz | Drafts |
|---|---|---|---|
| nlohmann/json | 3.12.0 | MIT | – |
| pboettch/json-schema-validator | 2.4.0 | MIT | Draft 7 |
| valijson | 1.1.3 | BSD-2 | Draft 7 |
| jsoncons | 1.10.0 | BSL-1.0 | 4–2020-12 (eigener JSON-Typ) |
Empfehlung: nlohmann + pboettch; Schemas nur im gemeinsamen Subset (type, properties, required,
`additionalProperties:false`, enum, minimum/maximum, items, anyOf, interne `$ref/$defs`). Fehlertexte prüfen (Mini-Spike).

## 5. Headless
- `ScopedJuceInitialiser_GUI` in `main`, ohne Message-Loop; gezielt pumpen (`runDispatchLoopUntil`,
  `dispatchPendingUpdatesSynchronously`). Eigenes `UIBehaviour` (keine Dialoge), `EngineBehaviour` ohne Auto-Geräte.
  Je Aufruf eigene Engine (`TRACKTION_ENABLE_SINGLETONS=0`). Abbau: Engine vor Initialiser.

## 6. Geräte
- `engine.getDeviceManager().deviceManager` = `juce::AudioDeviceManager`: Typ wählen, `setAudioDeviceSetup`, Kanäle.
  `JUCE_JACK=1`, `JUCE_ASIO=1` nötig. Zustand über `PropertyStorage` (`audio_device_setup` u. a.) → eigene
  `PropertyStorage`-Subklasse mit eigener Settings-Datei (atomar), Tests in-memory.
- Headless: Hosted Device (Engine/Routing) + **Fake-`AudioIODeviceType`** über `addAudioDeviceType` für Auswahl/Persistenz
  `[VERIFIZIEREN]`. Echte Backends = Handtest des PO.

## Lead-Entscheidungen (Rev 3)
1. Nur `Edit` + eigener Projektordner-Code (kein `Project`/`ProjectManager`).
2. Eigenes atomisches Speichern, eigene `tracklabFormatVersion`, Migration auf dem ValueTree.
3. nlohmann/json 3.12.0 + pboettch/json-schema-validator 2.4.0 (Submodule); Schema-Subset-Regel im Registry-Test erzwungen.
4. Undo-Tiefe 200 (einstellbar).
5. `modifiedBy`/`getUserName()` = fester Wert `"Tracklab"` – keine Personendaten in Projektdateien.
6. Gate-Tests für Geräte: Hosted Device + Fake-Gerätetyp; echte Backends als Prüfaufgabe (V) für den PO.
