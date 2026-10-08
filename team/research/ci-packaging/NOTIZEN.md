# CI und Packaging – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
Einschränkung: clang.llvm.org, apt.llvm.org, jrsoftware.org, docs.github.com, nsis.sourceforge.io gesperrt; Ersatz über
raw.githubusercontent.com der Primär-Repos und packages.ubuntu.com.

## 1. Runner (actions/runner-images README)
- `ubuntu-24.04` = `ubuntu-latest`; **`ubuntu-26.04`** (+ `-arm`) verfügbar (GA); `windows-2025` = `windows-latest`
  (+ `windows-2025-vs2026`); `windows-2022`; ARM: `ubuntu-24.04-arm`, `windows-11-arm`.
- Empfehlung: Labels pinnen (`ubuntu-24.04`, `windows-2025`) – `gate.yml` nutzt heute noch `windows-latest`.

## 2. Vorinstalliert (Image-Readmes, Stand 27.09.2026)
- Ubuntu 24.04: Clang 16/17/18 (Default 18), GCC 12/13/14, CMake 3.31.6, Ninja 1.13.2, Xvfb, dpkg-dev, fakeroot, patchelf;
  **fehlt:** ccache, libfuse2, desktop-file-utils, JUCE-Dev-Pakete.
- Ubuntu 26.04: Clang 20/21/22, GCC 13/14/15, CMake 4.4.3.
- Windows 2025: VS 2022 17.14 (inkl. clang-cl), LLVM 20.1.8, Windows SDK 10.0.26100, CMake 3.31.6, **Inno Setup 6.7.1**,
  WiX 3.14; NSIS fehlt.

## 3. RealtimeSanitizer
- **Ab Clang 20.1.0** (`-fsanitize=realtime`); Function Effects `nonblocking`/`nonallocating` mit `-Wfunction-effects`
  (+ `-Wperf-constraint-implies-noexcept`). Quelle: llvmorg-20.1.0 `ReleaseNotes.rst`, `FunctionEffectAnalysis.rst`.
- `RTSAN_OPTIONS` (`halt_on_error`, `suppressions`, `verify_interceptors` …), `__rtsan::ScopedDisabler`.
  Achtung: spät geladene Runtime (`dlopen`, Plugin-Host) → `verify_interceptors` abschalten.
- Windows-Unterstützung **nicht belegt** → RTSan nur Linux/Clang ≥ 20.
- Clang ≥ 20 auf dem Runner: `ubuntu-26.04` (vorinstalliert) oder `ubuntu-24.04` + Paket `clang-20` aus noble
  (Laufzeit `libclang-rt-20-dev` `[VERIFIZIEREN]`); apt.llvm.org vermeiden.

## 4. Caching
- GitHub-Cache 10 GB je Repo, Verdrängung nach 7 Tagen ohne Zugriff; Restore vom eigenen Branch und Default-Branch.
- Linux: `hendrikmuhs/ccache-action`; Windows: `mozilla-actions/sccache-action` (v0.0.11) +
  `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` `[VERIFIZIEREN]` im Spike. Debug-/Release-Caches getrennt.

## 5. Linux-Pakete
- AppImage: linuxdeploy bzw. appimagetool (Versionen beim Einrichten pinnen, Downloads per Hash); in CI
  `APPIMAGE_EXTRACT_AND_RUN=1` oder `libfuse2t64`; `.desktop` + Icons aus `assets/branding/png/`. Bau auf 24.04 → Mint 22.
- .deb: CPack `DEB` mit `CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON`.

## 6. Windows
- **Inno Setup 6.7.x** vorinstalliert, permissive Lizenz, `notimestamp` für reproduzierbare Builds; Inno 7 erschienen
  (Lizenz `[VERIFIZIEREN]`). NSIS 3.11 (zlib) nur per `choco`. Portables ZIP per `cmake --install` + Archiv.

## 7. Artefakte und Release
- `actions/upload-artifact@v7`: Retention 1–90 Tage (Default 90). Empfehlung: Test-Builds 14 Tage, Releases nur als
  Release-Assets; Installer mit `compression-level: 0`.
- Release-Trigger per Glob `v[0-9][0-9][0-9][0-9].[0-9][0-9].*` + strikte Regex-Prüfung `^v[0-9]{4}\.[0-9]{2}\.[0-9]+$`;
  `gh release create`, `contents: write` nur im Release-Job. Tag setzt David (F7).

## 8. Code-Signing Windows (Überblick)
- OV/EV-Zertifikat: max. 460 Tage Laufzeit seit 01.03.2026, Hardware-Schlüssel, ca. 80–1.500 USD/Jahr (Händlerangaben).
- Microsoft Artifact Signing: Einzelpersonen nur USA/Kanada → in Österreich nur als Organisation; Preis `[VERIFIZIEREN]`.
- **SignPath Foundation:** kostenlos für OSS (OSI-Lizenz, öffentlich, automatischer Build) – passt zu AGPLv3 + öffentlichem Repo.

## Vorschlag Gate-/CI-Struktur (eingearbeitet in `team/board/backlog/M0-07.md`)
`CMakePresets.json`; `gate.sh static|build|rtsan|tidy|all`; `gate.yml` mit Jobs `static` → `build-test-linux`
(GCC 14, Clang 18) · `rtsan-linux` (Clang ≥ 20) · `build-test-windows` (MSVC, sccache); `packaging.yml` (`workflow_call`,
nur `main`/Release); `release.yml` (Tag). Umstellung schrittweise: static + GCC → MSVC → Clang/RTSan → Packaging.

## Offen für den PO (→ `TODO-PO.md`)
- F6 Inno Setup 6.7.x bestätigt; F7 unverändert A (+ Regex-Prüfung).
- **F29** Code-Signing Windows: Empfehlung vorerst keines, nach F2 SignPath Foundation.
- **F30** Plattformumfang nur x64; RTSan nur Linux (DESIGN §8 ergänzen).
