#!/usr/bin/env python3
"""Regenerate res/app.ico from res/logo.png (needs Pillow: pip install pillow)."""
from PIL import Image
import os
here = os.path.dirname(os.path.abspath(__file__))
src = os.path.join(here, "..", "res", "logo.png")
dst = os.path.join(here, "..", "res", "app.ico")
im = Image.open(src).convert("RGBA")
im.save(dst, format="ICO", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
print("OK ->", dst)
