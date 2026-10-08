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
- Audio-Callbacks und DSP-Prozess-Funktionen sind `[[clang::nonblocking]]` annotiert (im Code über das Makro
  `SPIKE_NONBLOCKING`, das auf GCC/MSVC/Clang < 19 leer ist) und `noexcept` (Clang 20 verlangt das). Die Annotation
  steht hinter `noexcept`: `void processBlock (...) noexcept [[clang::nonblocking]] override;`.
- Eigener Gate-Schritt (`scripts/gate.sh rtsan`, in CI Job `rtsan-linux`): Preset `linux-clang-rtsan`
  (`-fsanitize=realtime`, Clang ≥ 20.1, nur Linux – für Windows ist RTSan nicht belegt) führt alle Engine-Tests aus.
  Jeder Befund beendet den Test (`halt_on_error`) und blockiert das Gate. Lokal ohne Clang ≥ 20 wird die Stufe
  übersprungen, in CI (`GATE_REQUIRE_RTSAN=1`) ist ein fehlender Clang ein Fehler.
- `-Wfunction-effects` (Clang ≥ 20, in `-Werror` enthalten) prüft die annotierten Funktionen zur Übersetzungszeit.
  Aufrufe in Systemheader und in JUCE/Tracktion (als SYSTEM-Includes eingebunden) meldet es nicht; das fängt RTSan zur
  Laufzeit.

### RealtimeSanitizer: Regeln für Ausnahmen
- **Eigener Code hat keine Ausnahme.** Ein Befund in `spike::`, in unseren Plugins oder später in `src/` ist ein
  Fehler im Code, kein Fall für `scripts/rtsan.supp`. `__rtsan::ScopedDisabler` ist ebenfalls nicht erlaubt.
- **Fremdcode** (Tracktion, JUCE) steht in `scripts/rtsan.supp` als `call-stack-contains:<innerste Tracktion-Funktion>`,
  je Eintrag mit Kommentar (was passiert, warum es vorerst unvermeidbar ist). Keine Namespace-Pauschalen
  (`tracktion::`, `juce::`): Der Stack enthält auch die Aufrufer, eine Pauschale würde unsere eigenen Callbacks
  verdecken, die Tracktion oder der JUCE-Plugin-Wrapper aufruft.
- **Grenze:** Ein neuer Befund unterhalb einer unterdrückten Tracktion-Funktion (vor allem der Einstiegspunkte
  `HostedAudioDevice::processBlock`, `DeviceManager::audioDeviceIOCallbackInternal`) wird mit unterdrückt. Eigene
  Callbacks werden deshalb zusätzlich ohne Tracktion im Stack geprüft (Plugin-Suite, Negativtest).
- Die Liste ist die Mängelliste von Tracktions Aufnahmepfad (Locks und Allokation im Callback, Spike-Bericht Abschnitt 7).
  Sie wird mit der Aufnahmekarte (M4-01) abgebaut; neue Einträge brauchen eine Begründung im Review.
- Neue Fundstellen finden: `RTSAN_OPTIONS=halt_on_error=false:detect_leaks=0` über `ctest` im Preset
  `linux-clang-rtsan`, dann die innerste Tracktion-Funktion je Befund eintragen.
- **Negativtest** `spike.rtsan_negative`: ein absichtlich allokierender `[[clang::nonblocking]]`-Callback muss RTSan
  auslösen. Der CTest-Wrapper (`spike/engine/cmake/rtsan_expect_failure.cmake`) verlangt Exit-Code ≠ 0 *und* einen
  RTSan-Befund im Callback; sonst ist das Gate rot (der Sanitizer wäre blind oder eine Suppression zu breit).
- Der RTSan-Lauf ist langsam (record12 etwa 2 Minuten statt 4 Sekunden), weil jeder unterdrückte Befund symbolisiert
  wird; die CTest-Timeouts sind für dieses Preset erhöht.
- Plugin-Sandbox: ein abstürzendes Plugin darf den Audio-Thread nicht mitreißen (Design §6.5).
