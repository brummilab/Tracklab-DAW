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
- `-Wfunction-effects` ist für eigene Quellen aktiv (CMake `spike_warnings`, nur Clang ≥ 20, mit `-Werror`). Es prüft jede
  `nonblocking`-Funktion zur Übersetzungszeit; Aufrufe in JUCE-/Tracktion-Header werden ebenfalls gemeldet.
- **Regel:** `[[clang::nonblocking]]` steht nur auf Funktionen, deren gesamte Aufrufkette eigener, geprüfter Code ist.
  Grenzfunktionen zu Fremdcode (z. B. `HostedDeviceDriver::feed`, simuliert den Hardware-Callback und ruft Tracktion)
  bekommen die Annotation nicht, sondern einen `spike::RealtimeScope`: RealtimeSanitizer prüft sie zur Laufzeit, der
  Compiler nicht.

### RealtimeSanitizer: Regeln für Ausnahmen
- **Eigener Code hat keine Ausnahme.** Ein Befund in `spike::`, in unseren Plugins oder später in `src/` ist ein
  Fehler im Code, kein Fall für `scripts/rtsan.supp`. `__rtsan::ScopedDisabler` ist ebenfalls nicht erlaubt.
- **Fremdcode** (Tracktion, JUCE) steht in `scripts/rtsan.supp` als `call-stack-contains:<innerste Tracktion-Funktion>`,
  je Eintrag mit Kommentar (was passiert, warum es vorerst unvermeidbar ist). Keine Namespace-Pauschalen
  (`tracktion::`, `juce::`): Der Stack enthält auch die Aufrufer, eine Pauschale würde unsere eigenen Callbacks
  verdecken, die Tracktion oder der JUCE-Plugin-Wrapper aufruft.
- **Grenze:** Ein Eintrag auf einen Einstiegspunkt (`HostedAudioDevice::processBlock`,
  `DeviceManager::audioDeviceIOCallbackInternal`) deckt den **ganzen** Tracktion-Pfad darunter ab, auch neue Befunde.
  Solange diese Einträge stehen, dürfen keine eigenen Nodes oder Plugins im Tracktion-Graph laufen, die RTSan prüfen
  soll: Die Einstiegspunkt-Suppressions müssen vorher durch Suppressions auf die innersten Fundstellen abgelöst werden.
  Eigene Callbacks werden bis dahin ohne Tracktion im Stack geprüft (Plugin-Suite, Negativtest).
- Die Liste ist die Mängelliste von Tracktions Aufnahmepfad (Locks und Allokation im Callback, Spike-Bericht Abschnitt 7).
  Sie wird mit der Aufnahmekarte (M4-01) abgebaut; neue Einträge brauchen eine Begründung im Review.
- Neue Fundstellen finden: `RTSAN_OPTIONS=halt_on_error=false:detect_leaks=0` über `ctest` im Preset
  `linux-clang-rtsan`, dann die innerste Tracktion-Funktion je Befund eintragen.
- **Negativtest** `spike.rtsan_negative`: ein absichtlich allokierender `[[clang::nonblocking]]`-Callback muss RTSan
  auslösen. Der CTest-Wrapper (`spike/engine/cmake/rtsan_expect_failure.cmake`) verlangt Exit-Code ≠ 0 *und* einen
  RTSan-Befund im Callback; sonst ist das Gate rot (der Sanitizer wäre blind oder eine Suppression zu breit).
- Der CI-Job `rtsan-linux` läuft auf `ubuntu-24.04` mit `clang-20` aus noble-updates (20.1.2). `ubuntu-26.04` geht noch nicht:
  Dort nutzt clang-20 die libstdc++ 15, und Tracktion develop kompiliert damit nicht (`std::make_shared<AudioClipPlayhead>()`,
  implizit gelöschter Konstruktor wegen `std::atomic<State>` in `tracktion_AudioClipBase.h`). Das betrifft auch Linux
  Mint 23 / Ubuntu 26.04 als Zielplattform, bis Tracktion nachzieht (Risiko für M1).
- Der RTSan-Lauf ist langsam (record12 etwa 2 Minuten statt 4 Sekunden), weil jeder unterdrückte Befund symbolisiert
  wird; die CTest-Timeouts sind für dieses Preset erhöht.
- Plugin-Sandbox: ein abstürzendes Plugin darf den Audio-Thread nicht mitreißen (Design §6.5).
