# GUI und Design-System – Notizen (M0-04)

Stand: 08.10.2026 · Lauf: `researcher` · Übernommen vom Lead.
Gelesen: JUCE 9.0.3 (`be29c81`); **Screenshot-Test gebaut und ausgeführt** (Scratchpad, nicht im Repo).
w3.org und jfly.uni-koeln.de gesperrt.

## 1. Token-Format `themes/*.json` (Vorschlag)
- Zwei Ebenen: `primitive` (Brand-Farben aus `assets/branding/README.md`, Abstände, Radien, Schrift) und `semantic`
  (Verweise `{pfad}`); Komponenten lesen nur `semantic`. Light, High-Contrast und farbenblindsichere Palette als
  `extends`-Overlays. Schema `themes/theme.schema.json` mit `additionalProperties: false`. Umschalten über `ui.set_theme`
  ohne Neustart; OS-Dark-Mode über `Desktop::isDarkModeActive()` möglich.
- Kontrast Dark (gegen `#0F172A`, selbst gerechnet): Text 14,4 · Amber 8,3 · Sky 8,3 · Teal 9,6 · Indigo 6,0.
- **Light-Theme:** Brand-Farben auf Weiß zu schwach (Amber 2,15:1) → abgedunkelte Varianten: Amber `#B45309` (5,0),
  Sky `#0369A1` (5,9), Teal `#0F766E` (5,5), Indigo `#4F46E5` (6,3). Logo bleibt unverändert.
- Meter-Stops Vorschlag: −60 dB Teal, −18 Sky, −6 Amber, 0 Rot `#EF4444`.
- Beispieldatei: siehe Researcher-Bericht im Chat-Verlauf; wird mit der ersten GUI-Karte als `themes/dark.json` angelegt.

## 2. Renderer, HiDPI, SVG, Fonts (JUCE 9.0.3)
- Windows: GDI und **Direct2D (Standard)**, umschaltbar per `ComponentPeer::setCurrentRenderingEngine` (Fallback).
- Linux: Fenster nur **Software-Renderer**; OpenGL/OpenGL ES über EGL (`libegl-dev`) nur per `OpenGLContext`.
  **Kein Wayland-Backend** → läuft unter X11/XWayland.
- HiDPI: Windows per-monitor DPI-aware; Linux liest Skalierung aus dconf/gsettings/DPI; Nutzer-Skalierung über
  `Desktop::setGlobalScaleFactor`. 300 % `[VERIFIZIEREN]`.
- SVG: neuer Parser auf Basis lunasvg (MIT), `Drawable::createFromSVGFile/String`, headless nutzbar; alte XML-API entfernt.
  Einfärben (`replaceColour`/`currentColor`) `[VERIFIZIEREN]`.
- Variable Fonts seit 9.0.0: `Font::withVariableSettings`, Laden aus BinaryData über `Typeface::createSystemTypefaceFor`.

## 3. Offscreen-Screenshots – praktisch geprüft
- `Component::createComponentSnapshot(area, clip, scale, SoftwareImageType())` braucht kein Fenster.
- **Test: ohne `DISPLAY` und ohne Xvfb → PNG 400×200, Exit 0.** Gebaut mit `JUCE_USE_FONTCONFIG=0` und ohne X-Erweiterungen.
- Empfehlung: immer `SoftwareImageType()` (plattformgleich); CLI rendert nur Komponentenbäume, keine nativen Fenster
  (Menüs/Tooltips fehlen). Xvfb nur als Fallback. Inter aus BinaryData macht Screenshots schriftunabhängig.

## 4. Docking / Screensets
- JUCE hat kein Docking (nur Tabs, Splitter, `ResizableWindow`-Zustand). Fremdlösungen klein/alt (`christopherbaine/docks`
  MIT, 27 Commits; JUCETICE GPL unklar). Suche unvollständig (GitHub-Suche gesperrt).
- Empfehlung: **eigener Split/Tab-Baum** (`DockNode`), Layout als JSON, Screenset = benanntes Layout, über Registry
  steuerbar. Aufwand: Drop-Zonen, Tab-Tear-off, Mehrmonitor.

## 5. Barrierefreiheit
- JUCE-Doku (`docs/Accessibility.md`): Screenreader **nur Windows (Narrator/UIA)**, macOS, iOS, Android – **kein AT-SPI
  unter Linux** (leere Implementierung). NVDA/JAWS `[VERIFIZIEREN]`.
- Tastaturbedienung und Fokus-Traversierung plattformunabhängig; Fokusring als Token.
- Kontrastthema ≥ 7:1 Text, ≥ 3:1 UI-Kanten (WCAG aus Fachwissen `[VERIFIZIEREN]`).
- Farbenblindsichere Meter: nicht nur Farbe (Marken bei −18/−6/0 dB, Clip als Form/Text); Palette `cvd-safe` nach Okabe-Ito
  (`#56B4E9` → `#F0E442` → `#E69F00` → `#D55E00`) `[VERIFIZIEREN]`.

## 6. Inter
- OFL 1.1 (`rsms/inter/LICENSE.txt`), OFL-Text + Copyright in Third-Party-Notices. README nennt „Inter“ als Reserved Font
  Name → **nicht subsetten** (sonst umbenennen). Version v4.1; Dateiname/Größe `InterVariable.ttf` `[VERIFIZIEREN]`.
  Variable Font ungekürzt einbetten, Rückfall drei statische Schnitte.

## Folgen für den Lead
- Auftrag §5/§8 und dieses README sprechen von „JUCE 8“ → in DESIGN Rev 2 auf JUCE 9 aktualisieren (vorbehaltlich F24).
- DESIGN §5: Screenreader nur Windows; Screenshots ohne Xvfb.

## Offen für den PO (→ `TODO-PO.md`)
- **F37** Linux: kein Screenreader in v1 (nur Tastatur, Kontrast, Skalierung); Wayland nur über XWayland.
- **F38** Docking als Eigenbau; Light-Theme mit abgedunkelten Akzentfarben (Optik bestätigen).
