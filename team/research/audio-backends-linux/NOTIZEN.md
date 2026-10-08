# Audio-Backends (Linux und Windows) – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
Gelesen: JUCE 9.0.3 (`be29c81`), Tracktion `develop` (`bb38617`), PipeWire 1.0.5 (`a2287be`), packages.ubuntu.com (noble).
Gesperrt: linuxmint.com, learn.microsoft.com, docs.pipewire.org → Mint- und Microsoft-Aussagen nur sekundär. Nichts auf
Hardware gemessen.

## 1. Mint, PipeWire, Echtzeit
- Mint 22.x nutzt PipeWire als Standard (sekundär: Phoronix 03/2024, OMG!Ubuntu 04/2024). Aktuell **Mint 22.3 „Zena“**
  (13.01.2026). **Mint 23 nicht erschienen** (erwartet Dezember 2026, Basis Ubuntu 26.04) → Ziel bleibt Mint 22/Ubuntu 24.04.
- PipeWire in noble: **1.0.5**; Pro-Audio-IRQ-Scheduling seit 0.3.81 Standard (`NEWS`), Profil `pro-audio` ohne Resampling.
- `pipewire-jack` legt `libjack.so.0` nur unter `/usr/lib/x86_64-linux-gnu/pipewire-0.3/jack/` ab; im Standardpfad liegt
  jackd2. JUCE lädt `libjack.so.0` per `dlopen` mit `JackNoStartServer` → **Start über `pw-jack tracklab`** (oder
  `ld.so.conf.d`-Eintrag systemweit).
- RT-Rechte: PipeWire `module-rt` nutzt `RLIMIT_RTPRIO` oder RTKit (Prio max. 20). Ubuntu noble liefert
  `25-pw-rlimits.conf` **nicht** aus.
- **JUCE-ALSA-Thread bekommt unter Linux keine Echtzeit-Priorität** (`Thread::setPriority` ist No-op,
  `juce_Threads_linux.cpp:72-80`). Beim JACK-Backend läuft der Callback im PipeWire-Datenthread mit RT-Priorität.

## 2. JUCE ALSA vs. JACK über PipeWire
- `JUCE_JACK` Default **0** → Tracklab setzt `JUCE_JACK=1` (Build braucht `libjack-jackd2-dev`, Laufzeit per `dlopen`).
- JACK in JUCE: Ein-/Ausgang getrennt wählbar, eigene Ports `in_N`/`out_N`; **Puffergröße/Samplerate bestimmt PipeWire**,
  nicht `open()` → eigene Einstellung über `pw-metadata -n settings 0 clock.force-quantum N` bzw. `PIPEWIRE_QUANTUM`.
  Latenz über `jack_port_get_total_latency` `[VERIFIZIEREN]` per Loopback.
- ALSA: PipeWire hält die Karte (`hw:` vermutlich „busy“); JUCE-Workaround klemmt Kanäle bei PipeWire 1.6.0–1.6.2 auf 64
  (Mint 22 mit 1.0.5 nicht betroffen).
- 12+ Eingänge: beide Wege; Pro-Audio-Profil nötig, damit alle Kanäle einzeln erscheinen (Vermutung).
- Aggregat-Geräte: weder JUCE noch PipeWire (nur `module-combine-stream`) → MVP ein Interface.

## 3. Windows
- **ASIO-SDK dual: Steinberg-Lizenz oder GPLv3** (`juce_audio_devices/native/asio/LICENSE.txt`, JUCE 9.0.3; Steinberg
  15.10.2025, sekundär). Passt zu AGPLv3. „ASIO“-Markenrichtlinie beachten. `JUCE_ASIO` Default 0.
- WASAPI in JUCE 9: `WASAPIDeviceMode { shared, exclusive, sharedLowLatency }` (`IAudioClient3`); alle drei standardmäßig
  angelegt. Empfehlung: ASIO bevorzugt, WASAPI exklusiv als Fallback.

## 4. Latenz und Aufnahme-Kompensation
- JUCE: nur gemeldete Gerätelatenz, kein Loopback-Test.
- Tracktion: `DeviceManager::getRecordAdjustmentSamples()` = In + Out, automatisch angewendet; manueller Offset je Eingang
  `WaveInputDevice::setRecordAdjustmentMs` (±500 ms). Clip-Start um `-adjust + blockSize` verschoben (Kommentar „not quite
  sure why“) → Vorzeichen/Blockversatz `[VERIFIZIEREN]`.
- Loopback-Tester `test_utilities::LatencyTester` (`tracktion_RoundTripLatency.h`) existiert, ist aber intern → nachbauen
  oder direkt einbinden. Grundlage für `io.measure_latency`.

## 5. sudo-Schritte für David (Mint 22, ungetestet)
- Vorab ohne sudo: `pactl info | grep "Server Name"`, `dpkg -l pipewire pipewire-jack wireplumber rtkit libjack-jackd2-0`, `id`.
- Laufzeit: `sudo apt install pipewire pipewire-audio pipewire-jack wireplumber rtkit`.
- Optional RT-Limits über Gruppe `audio` (`/etc/security/limits.d/25-tracklab-audio.conf`: rtprio 95, nice −19, memlock 4194304).
- Optional `pipewire-jack` systemweit über `/etc/ld.so.conf.d/` + `ldconfig`.
- Build-Pakete: siehe `engine-spike/NOTIZEN.md` §8.
Wird erst bei Bedarf (lokale Entwicklung/Tests am Laptop) als USER INPUT REQUIRED gestellt.

## Offen für den PO (→ `TODO-PO.md`)
- **F36** Linux-Standard-Backend: JACK-API über PipeWire (`pw-jack`) statt ALSA direkt.
- F9 (Audio-Interface) bleibt wichtig: class-compliant unter Linux? ASIO-Treiber unter Windows? Kanalzahl?
- Aggregat-Geräte nicht im MVP; Mint 23 erst nach Erscheinen testen; ASIO über GPLv3-Option (ADR).
- Messungen im Spike: Loopback-Latenz 64/128/256, Vorzeichen `recordAdjustMs`, Portnamen bei 12+ Eingängen.
