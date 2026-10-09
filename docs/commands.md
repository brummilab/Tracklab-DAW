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
| `project.get_info` | `project_get_info` | Projektinfo abfragen | Returns path, name, format version and whether there are unsaved changes of the open project. Fails with no_edit if no project is open. | readOnly | - | - |
| `project.new` | `project_new` | Neues Projekt | Creates a new project <folder>/<name>/<name>.tracklab with the sub folders Audio, Renders, Backups and Peaks and opens it. Fails with unsaved_changes if the open project has unsaved changes. | - | Ctrl+N | Datei |
| `project.open` | `project_open` | Projekt öffnen | Opens a .tracklab project file. Fails with unsaved_changes if the open project has unsaved changes, with project_not_found, corrupt_project or project_too_new if the file cannot be opened. | - | Ctrl+O | Datei |
| `project.save` | `project_save` | Projekt speichern | Saves the open project to its file (atomically: the old file stays intact if saving fails). Fails with no_edit if no project is open. | - | Ctrl+S | Datei |
| `project.save_as` | `project_save_as` | Projekt speichern unter | Saves the open project as a new project <folder>/<name>/<name>.tracklab and continues in it. Media files are not copied. Never overwrites an existing project. | - | Ctrl+Shift+S | Datei |
