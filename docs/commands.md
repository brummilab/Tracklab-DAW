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

## io

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `io.get_device` | `io_get_device` | Aktuelles Audiogerät anzeigen | Returns the open audio device: driver type, input and output device, sample rate, buffer size and active channels. 'open' is false when no device is open. | readOnly | - | - |
| `io.list_device_types` | `io_list_device_types` | Audio-Treibertypen auflisten | Lists the audio driver types (e.g. ALSA, JACK, WASAPI, ASIO) with the number of devices and the type of the open device. 'hints' tells the user how to make an empty type work. | readOnly | - | - |
| `io.list_devices` | `io_list_devices` | Audiogeräte auflisten | Lists the audio devices per driver type: input and output devices with channel names, sample rates and buffer sizes. Optional 'type' limits the list to one driver type. | readOnly | - | - |
| `io.set_device` | `io_set_device` | Audiogerät einstellen | Selects and opens the audio device: driver type, input and output device, sample rate, buffer size and active channels (0-based indices). Omitted values stay as they are; an empty device name means none. Fails without changing anything if a device, rate, size or channel is not available. Returns the new setup. The setting is stored. | - | - | - |

## project

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `project.close` | `project_close` | Projekt schließen | Closes the open project. Fails with unsaved_changes if there are unsaved changes, unless discard is true, which throws them away. Does nothing if no project is open. | destructive | Ctrl+W | Datei |
| `project.discard_autosave` | `project_discard_autosave` | Autosave verwerfen | Deletes the autosave file of the open project and ends an offered recovery; the project in memory stays as it is. Fails with no_autosave or no_edit. | destructive | - | - |
| `project.get_info` | `project_get_info` | Projektinfo abfragen | Returns path, name, format version and whether there are unsaved changes of the open project. Fails with no_edit if no project is open. | readOnly | - | - |
| `project.list_backups` | `project_list_backups` | Backups anzeigen | Lists the rotating backups of the open project (one per save, the oldest are deleted), newest first. Fails with no_edit if no project is open. | readOnly | - | - |
| `project.new` | `project_new` | Neues Projekt | Creates a new project <folder>/<name>/<name>.tracklab with the sub folders Audio, Renders, Backups and Peaks and opens it. Fails with unsaved_changes if the open project has unsaved changes. | - | Ctrl+N | Datei |
| `project.open` | `project_open` | Projekt öffnen | Opens a .tracklab project file. Fails with unsaved_changes if the open project has unsaved changes, with project_not_found, corrupt_project (the message names the newest backup or autosave) or project_too_new if the file cannot be opened. If an autosave newer than the project file exists the result has recovery_available with both times: the project is opened as the file has it, decide with project.restore_autosave or project.discard_autosave. | - | Ctrl+O | Datei |
| `project.restore_autosave` | `project_restore_autosave` | Autosave wiederherstellen | Loads the autosave file of the open project as its current state (recovery after a crash), replacing the state in memory. The project file is only replaced by the next save, which also deletes the autosave. Fails with no_autosave or no_edit. | destructive | - | - |
| `project.restore_backup` | `project_restore_backup` | Backup wiederherstellen | Loads a backup of the open project as its current state, replacing the state in memory including unsaved changes. Makes a backup of the current state first. The project file is only replaced by the next save. Fails with backup_not_found, no_edit or invalid_params (name is not a plain file name). | destructive | - | Datei |
| `project.save` | `project_save` | Projekt speichern | Saves the open project to its file (atomically: the old file stays intact if saving fails). Fails with no_edit if no project is open. | - | Ctrl+S | Datei |
| `project.save_as` | `project_save_as` | Projekt speichern unter | Saves the open project as a new project <folder>/<name>/<name>.tracklab and continues in it. Media files are not copied. Never overwrites an existing project. | - | Ctrl+Shift+S | Datei |
