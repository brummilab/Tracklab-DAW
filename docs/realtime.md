# Echtzeit-Regeln (MUSS)

Gilt für jeden Code, der vom Audio-Thread erreichbar ist. Der reviewer prüft das als Pflichtfrage 1.

## Im Audio-Thread verboten
- Speicher allozieren oder freigeben (`new`, `delete`, `malloc`, wachsende Container, `std::string`-Kopien)
- Locks, Mutexe, Condition Variables, `std::shared_ptr`-Kopien mit atomarem Refcount auf geteilten Objekten
- Datei-, Netzwerk- oder Konsolen-IO; Logging nur in einen Lock-free-Ringpuffer
- Exceptions werfen oder fangen
- Blockierende Systemaufrufe (Sleep, Warten auf andere Threads)

## Erlaubte Kommunikation
- SPSC/MPSC-Lock-free-Queues, atomare Snapshots (Pointer-Swap)
- Speicher wird auf einem Hintergrund-Thread freigegeben (Garbage-Queue)
- Änderungen am Engine-Graph: Commands laufen auf dem Message-Thread und übergeben lock-frei an den Audio-Thread

## Prüfung
- Audio-Callbacks und DSP-Prozess-Funktionen sind `[[clang::nonblocking]]` annotiert.
- Eigener CI-Job (Linux, Clang) mit RealtimeSanitizer (`-fsanitize=realtime`) über Engine-Tests und
  Fixture-Renders. Jeder Befund blockiert das Gate. `[VERIFIZIEREN: minimale Clang-Version für RTSan]`
- Plugin-Sandbox: ein abstürzendes Plugin darf den Audio-Thread nicht mitreißen (Design §6.5).
