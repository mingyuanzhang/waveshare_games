#!/usr/bin/env python3
"""Convert native renderer frames to PNGs with the physical display's round mask.

Optional: requires Pillow (python3 -m pip install Pillow).
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

directory = Path(__file__).resolve().parent.parent / ".build/preview"
names = ("intro", "idle", "ping", "menu", "rooms", "escaped")
sheet = Image.new("RGB", (1140, 820), "#18202B")
font = ImageFont.load_default(size=18)
mask = Image.new("L", (360, 360), 0)
ImageDraw.Draw(mask).ellipse((0, 0, 359, 359), fill=255)
for source in directory.glob("*.ppm"):
    Image.open(source).save(source.with_suffix(".png"))
for index, name in enumerate(names):
    frame = Image.open(directory / (name + ".ppm")).convert("RGB")
    frame.save(directory / (name + ".png"))
    x, y = (index % 3) * 380 + 10, (index // 3) * 410 + 10
    sheet.paste(frame, (x, y), mask)
    ImageDraw.Draw(sheet).text((x + 180, y + 378), name.upper(),
                             font=font, anchor="mm", fill="#BDD0E0")
sheet.save(directory / "screens.png")
print(directory / "screens.png")
