#!/usr/bin/env python3
"""
Download Serebii Pokearth Hisui tiles and stitch them into single PNGs
(one per region) matching the filenames pkTeraRaid expects.

Usage:
    python3 tools/fetch_pla_maps.py

Output:
    romfs/maps/obsidian_fieldlands.png   (1024x1024)
    romfs/maps/crimson_mirelands.png
    romfs/maps/cobalt_coastlands.png
    romfs/maps/coronet_highlands.png
    romfs/maps/alabaster_icelands.png

Source:
    https://www.serebii.net/pokearth/hisui/{region}/tile_{z}-{x}-{y}.png
    Zoom 2 → 4x4 grid of 256px tiles → 1024x1024 composite.

NOTE: Serebii's tiles are not redistributed under a permissive license. The
pkTeraRaid repo does not ship them. This tool downloads them to your local
checkout for personal use; do as you see fit.
"""

import io
import os
import sys
import time
import urllib.request

from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
OUT_DIR = os.path.join(PROJECT_DIR, "romfs", "maps")

BASE_URL = "https://www.serebii.net/pokearth/hisui"
USER_AGENT = "pkTeraRaid/tools"
TILE_SIZE = 256
ZOOM = 2           # maxZoom per PLA-Live-Map templates/map.html
GRID = 8           # tile grid size at ZOOM (confirmed against Serebii: tiles 0..7 in each axis)
OUTPUT_SIZE = 1024 # downscale from 2048x2048 native to this for on-device footprint

REGIONS = [
    ("obsidianfieldlands", "obsidian_fieldlands.jpg"),
    ("crimsonmirelands",   "crimson_mirelands.jpg"),
    ("cobaltcoastlands",   "cobalt_coastlands.jpg"),
    ("coronethighlands",   "coronet_highlands.jpg"),
    ("alabastericelands",  "alabaster_icelands.jpg"),
]
JPEG_QUALITY = 85

def fetch_tile(region, z, x, y):
    url = f"{BASE_URL}/{region}/tile_{z}-{x}-{y}.png"
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return Image.open(io.BytesIO(resp.read())).convert("RGBA")

def stitch(region):
    canvas = Image.new("RGBA", (GRID * TILE_SIZE, GRID * TILE_SIZE), (0, 0, 0, 0))
    for x in range(GRID):
        for y in range(GRID):
            try:
                tile = fetch_tile(region, ZOOM, x, y)
            except Exception as e:
                print(f"    tile {ZOOM}/{x}/{y} failed: {e}")
                continue
            canvas.paste(tile, (x * TILE_SIZE, y * TILE_SIZE))
            time.sleep(0.05)  # be polite
    return canvas

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for region, out_name in REGIONS:
        out_path = os.path.join(OUT_DIR, out_name)
        print(f"[{region}] stitching {GRID}x{GRID} tiles -> {out_path}")
        img = stitch(region)
        if OUTPUT_SIZE != img.size[0]:
            img = img.resize((OUTPUT_SIZE, OUTPUT_SIZE), Image.LANCZOS)
        img.convert("RGB").save(out_path, "JPEG", quality=JPEG_QUALITY, optimize=True)
        print(f"  saved {img.size[0]}x{img.size[1]}")
    print("done.")

if __name__ == "__main__":
    sys.exit(main() or 0)
