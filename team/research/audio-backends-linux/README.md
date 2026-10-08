# Audio-Backends (Linux und Windows)

## Fragen
1. Linux Mint 22.x: PipeWire als Standard, pipewire-jack, Echtzeit-Rechte (rtkit, `audio`-Gruppe, limits) `[VERIFIZIEREN]`
2. ALSA direkt vs. JACK-API über PipeWire: Latenz, Gerätewahl, Mehrkanal (12+ Eingänge)
3. Windows: ASIO-SDK-Lizenz (GPLv3-Option) `[VERIFIZIEREN]`, WASAPI exklusiv/geteilt
4. Latenzmessung per Loopback; Kompensation der Aufnahme
5. Was braucht David an sudo-Schritten auf Mint (für `USER INPUT REQUIRED`)?

## Quellen
- https://pipewire.org/ und PipeWire-Wiki
- Linux Mint Release Notes
- Steinberg ASIO SDK, Microsoft WASAPI-Doku
- JUCE AudioIODeviceType-Doku
