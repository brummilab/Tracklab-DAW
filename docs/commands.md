# Command-Referenz

> Diese Datei wird automatisch aus der Command-Registry erzeugt (`tracklab-cli export-tools`) und in der CI auf
> Aktualität geprüft. Nicht von Hand bearbeiten.

## app

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `app.version` | `app_version` | Version anzeigen | Returns the version of Tracklab. | readOnly | - | - |

## edit

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `edit.get_undo_state` | `edit_get_undo_state` | Undo-Status abfragen | Returns whether undo and redo are possible and the names of the next undo and redo step. | readOnly | - | - |
| `edit.redo` | `edit_redo` | Wiederholen | Redoes the last undone step of the project. Does nothing (done=false) if there is none. | - | Ctrl+Shift+Z | Bearbeiten |
| `edit.undo` | `edit_undo` | Rückgängig | Undoes the last undo step of the project. Does nothing (done=false) if there is none. | - | Ctrl+Z | Bearbeiten |
