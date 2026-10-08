# Tracklab – Branding

Logo und App-Icon für Tracklab. Im Repo liegt alles unter `assets/branding/`.

## Dateien

| Datei | Zweck |
|---|---|
| `tracklab-icon.svg` | App-Icon (Quelle, 512 × 512) |
| `tracklab-logo-dark.svg` | Logo mit Schriftzug für dunkle Hintergründe (Standard-Theme) |
| `tracklab-logo-light.svg` | Logo mit Schriftzug für helle Hintergründe |
| `tracklab.ico` | Windows-Icon (16–256 px) für EXE und Installer |
| `png/tracklab-icon-<n>.png` | Icon in 16, 24, 32, 48, 64, 128, 256, 512, 1024 px (Linux: AppImage/.deb, 256 und 512) |
| `png/tracklab-logo-*.png` | Logos als PNG, 1200 px breit (README, Splash, About-Dialog) |
| `render-icons.py` | erzeugt PNGs und ICO aus den SVGs neu (`pip install cairosvg pillow`) |

Die SVGs sind die Quelle. Nach einer Änderung `python3 render-icons.py` ausführen und alles committen.

## Idee
Vier Spuren mit versetzten Clips wie im Arrange-Fenster, darüber ein Playhead in Bernstein. Der Schriftzug „track“ ist neutral, „lab“ steht in der Akzentfarbe.

## Farben (Design-Tokens)

| Token | Wert | Verwendung |
|---|---|---|
| `brand.bg.top` | `#1E2A47` | Icon-Verlauf oben links |
| `brand.bg.bottom` | `#0F172A` | Icon-Verlauf unten rechts, dunkler Schriftzug |
| `brand.sky` | `#38BDF8` | Clips |
| `brand.teal` | `#2DD4BF` | Clips |
| `brand.indigo` | `#818CF8` | Clips |
| `brand.amber` | `#F59E0B` | Playhead, „lab“, Akzentfarbe der App |
| `brand.text.light` | `#E5E7EB` | Schriftzug auf dunklem Grund |

## Schrift
Schriftzug in Inter Bold (SIL Open Font License), in den SVGs bereits in Pfade umgewandelt. Es wird keine Schriftdatei benötigt.
