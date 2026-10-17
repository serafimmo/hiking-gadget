#!/usr/bin/env python3
"""Golden images (SPEC §10.1): compare the renders with tools/host/golden/*.pbm, or accept them.

usage: golden.py compare <render_dir> <golden_dir> <diff_dir>
       golden.py update  <render_dir> <golden_dir>

Renders are named NN_name.pbm; golden files are keyed by name only, so adding a fixture does not renumber them.
compare fails on any changed, new or removed screen and writes <diff_dir>/name.png (golden | render | changed
pixels in red) for a human to review before 'make golden-update'.
"""
import pathlib
import re
import shutil
import sys

from PIL import Image, ImageDraw

SCALE = 3
LIT, DARK, CHANGED, GAP = (150, 220, 255), (8, 10, 14), (255, 60, 60), (40, 40, 44)


def by_name(render_dir):
    out = {}
    for p in sorted(render_dir.glob("*.pbm")):
        m = re.match(r"\d+_(.+)$", p.stem)
        out[m.group(1) if m else p.stem] = p
    return out


def bits(path):
    data = path.read_bytes()
    parts = data.split(b"\n", 2)
    if parts[0].strip() != b"P4":
        raise ValueError(f"{path}: not a binary PBM")
    w, h = (int(v) for v in parts[1].split())
    raw, row = parts[2], (w + 7) // 8
    return w, h, [[bool(raw[y * row + x // 8] & (0x80 >> (x % 8))) for x in range(w)] for y in range(h)]


def diff_png(old, new, path):
    w, h, a = bits(old)
    _, _, b = bits(new)
    img = Image.new("RGB", ((3 * w + 2 * 4) * SCALE, h * SCALE), GAP)
    d = ImageDraw.Draw(img)
    changed = 0
    for y in range(h):
        for x in range(w):
            cells = ((0, LIT if a[y][x] else DARK), (w + 4, LIT if b[y][x] else DARK))
            for ox, c in cells:
                d.rectangle([(ox + x) * SCALE, y * SCALE, (ox + x + 1) * SCALE - 1, (y + 1) * SCALE - 1], fill=c)
            c = CHANGED if a[y][x] != b[y][x] else (LIT if b[y][x] else DARK)
            changed += a[y][x] != b[y][x]
            ox = 2 * (w + 4)
            d.rectangle([(ox + x) * SCALE, y * SCALE, (ox + x + 1) * SCALE - 1, (y + 1) * SCALE - 1], fill=c)
    img.save(path)
    return changed


def compare(render_dir, golden_dir, diff_dir):
    renders = by_name(render_dir)
    if not renders:
        print(f"golden: no renders in {render_dir} - run 'make render' first")
        return 1
    goldens = {p.stem: p for p in sorted(golden_dir.glob("*.pbm"))}
    if not goldens:
        print(f"golden: FAILED - no golden images in {golden_dir}; review the renders, then 'make golden-update'")
        return 1
    shutil.rmtree(diff_dir, ignore_errors=True)
    diff_dir.mkdir(parents=True)
    changed, new, gone = [], sorted(set(renders) - set(goldens)), sorted(set(goldens) - set(renders))
    for name in sorted(set(renders) & set(goldens)):
        if renders[name].read_bytes() != goldens[name].read_bytes():
            n = diff_png(goldens[name], renders[name], diff_dir / f"{name}.png")
            changed.append(f"{name} ({n} px)")
    for name, items in (("changed", changed), ("new (no golden)", new), ("removed (golden only)", gone)):
        if items:
            print(f"golden: {name}: {', '.join(items)}")
    if changed or new or gone:
        print(f"golden: FAILED - review {diff_dir}/*.png (golden | render | changed in red), "
              f"then 'make golden-update' if the change is intended")
        return 1
    print(f"golden: OK ({len(goldens)} screens identical)")
    return 0


def update(render_dir, golden_dir):
    renders = by_name(render_dir)
    if not renders:
        print(f"golden-update: no renders in {render_dir} - run 'make render' first")
        return 1
    golden_dir.mkdir(parents=True, exist_ok=True)
    gone = [p for p in golden_dir.glob("*.pbm") if p.stem not in renders]
    for p in gone:
        p.unlink()
    for name, p in renders.items():
        shutil.copyfile(p, golden_dir / f"{name}.pbm")
    print(f"golden-update: {len(renders)} screens accepted into {golden_dir}" + (f", {len(gone)} removed" if gone else ""))
    return 0


def main():
    a = sys.argv[1:]
    if len(a) == 4 and a[0] == "compare":
        return compare(*(pathlib.Path(x) for x in a[1:]))
    if len(a) == 3 and a[0] == "update":
        return update(*(pathlib.Path(x) for x in a[1:]))
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
