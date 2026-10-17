#!/usr/bin/env python3
"""PBM renders -> 4x PNGs with an OLED look, plus a labelled contact sheet (SPEC §10.1).

usage: pbm2png.py <dir>   (reads <dir>/*.pbm, writes <dir>/*.png and <dir>/contact_sheet.png)
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

SCALE = 4
LIT = (150, 220, 255)
DARK = (8, 10, 14)


def read_pbm(path):
    data = path.read_bytes()
    parts = data.split(b"\n", 2)
    if parts[0].strip() != b"P4":
        raise ValueError(f"{path}: not a binary PBM")
    w, h = (int(v) for v in parts[1].split())
    bits = parts[2]
    img = Image.new("RGB", (w * SCALE, h * SCALE), DARK)
    px = ImageDraw.Draw(img)
    row_bytes = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            if bits[y * row_bytes + x // 8] & (0x80 >> (x % 8)):
                x0, y0 = x * SCALE, y * SCALE
                px.rectangle([x0, y0, x0 + SCALE - 2, y0 + SCALE - 2], fill=LIT)
    return img


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    d = pathlib.Path(sys.argv[1])
    pbms = sorted(d.glob("*.pbm"))
    if not pbms:
        print(f"pbm2png: no .pbm files in {d}")
        return 1
    tiles = []
    for p in pbms:
        img = read_pbm(p)
        img.save(p.with_suffix(".png"))
        tiles.append((p.stem, img))
    cols = 4
    tw, th = tiles[0][1].size
    label_h = 18
    pad = 10
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (tw + pad) + pad, rows * (th + label_h + pad) + pad), (40, 40, 44))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for i, (name, img) in enumerate(tiles):
        cx = pad + (i % cols) * (tw + pad)
        cy = pad + (i // cols) * (th + label_h + pad)
        sheet.paste(img, (cx, cy))
        draw.text((cx, cy + th + 2), name, fill=(230, 230, 230), font=font)
    sheet.save(d / "contact_sheet.png")
    print(f"pbm2png: {len(tiles)} PNGs + contact_sheet.png in {d}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
