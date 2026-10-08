# Command-Referenz

> Diese Datei wird automatisch aus der Command-Registry erzeugt (`tracklab-cli export-tools`) und in der CI auf
> Aktualität geprüft. Nicht von Hand bearbeiten.

## app

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `app.version` | `app_version` | Version anzeigen | Returns the version of Tracklab. | readOnly | - | - |

## io

| ID | Tool-Name | Titel | Beschreibung | Flags | Shortcut | Menüpfad |
|---|---|---|---|---|---|---|
| `io.get_device` | `io_get_device` | Aktuelles Audiogerät anzeigen | Returns the open audio device: driver type, input and output device, sample rate, buffer size and active channels. 'open' is false when no device is open. | readOnly | - | - |
| `io.list_device_types` | `io_list_device_types` | Audio-Treibertypen auflisten | Lists the audio driver types (e.g. ALSA, JACK, WASAPI, ASIO) with the number of devices and the type of the open device. 'hints' tells the user how to make an empty type work. | readOnly | - | - |
| `io.list_devices` | `io_list_devices` | Audiogeräte auflisten | Lists the audio devices per driver type: input and output devices with channel names, sample rates and buffer sizes. Optional 'type' limits the list to one driver type. | readOnly | - | - |
| `io.set_device` | `io_set_device` | Audiogerät einstellen | Selects and opens the audio device: driver type, input and output device, sample rate, buffer size and active channels (0-based indices). Omitted values stay as they are; an empty device name means none. Fails without changing anything if a device, rate, size or channel is not available. Returns the new setup. The setting is stored. | - | - | - |
