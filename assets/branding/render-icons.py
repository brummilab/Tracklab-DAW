#!/usr/bin/env python3
"""Rendert die Tracklab-SVGs zu PNG-Icons und einer Windows-ICO.

Aufruf:  python3 render-icons.py [branding-ordner]
Benötigt: pip install cairosvg pillow
"""
import io
import os
import sys

import cairosvg
from PIL import Image

base = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
png_dir = os.path.join(base, "png")
os.makedirs(png_dir, exist_ok=True)

icon_svg = os.path.join(base, "tracklab-icon.svg")
sizes = [16, 24, 32, 48, 64, 128, 256, 512, 1024]
for s in sizes:
    cairosvg.svg2png(url=icon_svg, write_to=os.path.join(png_dir, f"tracklab-icon-{s}.png"),
                     output_width=s, output_height=s)

# Windows-ICO aus den gerenderten Größen (bis 256 px)
ico_sizes = [16, 24, 32, 48, 64, 128, 256]
imgs = [Image.open(os.path.join(png_dir, f"tracklab-icon-{s}.png")).convert("RGBA") for s in ico_sizes]
imgs[-1].save(os.path.join(base, "tracklab.ico"), sizes=[(s, s) for s in ico_sizes], append_images=imgs[:-1])

for variant in ("dark", "light"):
    cairosvg.svg2png(url=os.path.join(base, f"tracklab-logo-{variant}.svg"),
                     write_to=os.path.join(png_dir, f"tracklab-logo-{variant}.png"), output_width=1200)

print("PNG-Icons, Logos und tracklab.ico erzeugt in", base)
