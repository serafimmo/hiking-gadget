#!/usr/bin/env bash
# Static rules of SPEC §4.2 and §10.4. Comments are stripped first, so a rule never fires on documentation.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 -I - "$ROOT" <<'PY'
import pathlib, re, sys

ROOT = pathlib.Path(sys.argv[1])
SRC = ROOT / "src"
errors = []

def strip(text, keep_strings):
    """Remove comments (keeping line breaks); blank string/char literal contents unless keep_strings."""
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif text[i] in "\"'":
            q, j = text[i], i + 1
            while j < n and text[j] != q and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1] if keep_strings else q + q)
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)

def rel(p):
    return str(p.relative_to(ROOT))

def check(path, code, pattern, rule):
    for no, line in enumerate(code.splitlines(), 1):
        if re.search(pattern, line):
            errors.append(f"{rel(path)}:{no}: {rule}: {line.strip()}")

files = sorted(p for p in SRC.rglob("*") if p.suffix in (".h", ".cpp"))
for p in files:
    raw = p.read_text(encoding="utf-8", errors="replace")
    with_str = strip(raw, keep_strings=True)
    no_str = strip(raw, keep_strings=False)
    layer = p.relative_to(SRC).parts[0]
    r = rel(p)
    if layer in ("core", "drivers", "ui", "app"):
        check(p, with_str, r'#\s*include\s*[<"](Arduino\.h|Wire\.h|esp_[^">]*|driver/|freertos/)',
              "forbidden include outside hal (§4.2)")
        check(p, no_str, r'\b(millis|micros|delay|gettimeofday|settimeofday)\s*\(', "forbidden call outside hal (§4.2)")
        for no, line in enumerate(with_str.splitlines(), 1):
            for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', line):
                if any(not (0x20 <= ord(ch) <= 0x7E) for ch in lit):
                    errors.append(f"{r}:{no}: non-ASCII byte in a string literal (D29)")
    check(p, with_str, r'#\s*include\s*"(core|drivers|ui|app|hal|config)/',
          "include must be relative to the file (the Arduino builder has no -Isrc)")
    check(p, no_str, r'U8G2_\w*2ND_HW_I2C', "2ND_HW_I2C is a silent no-op on core 3.3.12 (§4.2)")
    if r != "src/hal/hal_clock.cpp":
        check(p, no_str, r'\b(settimeofday|configTime|sntp_\w*)\b', "wall clock is set only in hal_clock.cpp")
    if r != "src/ui/canvas.cpp":
        check(p, no_str, r'\bu8g2_DrawLine\b', "lines only through Canvas::line()")
    if layer != "hal":
        check(p, no_str, r'\bdelay\s*\(', "delay() outside hal")
    check(p, no_str, r'while\s*\(\s*!\s*Serial', "never wait for the USB host")
    check(p, no_str, r'\bESP_EXT1_WAKEUP_ALL_LOW\b', "deprecated alias (HWF F05)")
    check(p, no_str, r'#\s*warning\b', "use #error, not #warning (D35)")
    check(p, no_str, r'\bU8G2_R2\b', "use kOledFlip, never U8G2_R2 (§6.6)")
    if layer == "hal" and p.suffix == ".cpp":
        first = re.search(r'#\s*include\s*["<]([^">]+)[">]', with_str)
        if not first or first.group(1) != "build_checks.h":
            errors.append(f"{r}:1: the first include of a hal .cpp must be build_checks.h")

ino = ROOT / "Hiking_Gadget.ino"
for no, line in enumerate(strip(ino.read_text(encoding="utf-8"), keep_strings=True).splitlines(), 1):
    s = line.strip()
    if s and not re.match(r'#\s*include\b|void\s+(setup|loop)\s*\(\s*\)\s*\{.*\}$', s):
        errors.append(f"Hiking_Gadget.ino:{no}: only includes, setup() and loop() (ctags is stubbed): {s}")

for p in sorted((ROOT / "tools").rglob("*")):
    if p.suffix in (".h", ".cpp", ".ino") and p.is_file():
        check(p, strip(p.read_text(encoding="utf-8", errors="replace"), False), r'U8G2_\w*2ND_HW_I2C',
              "2ND_HW_I2C is a silent no-op on core 3.3.12 (§4.2)")

# The Web Serial page keeps its own copy of the zone table; it must match kTzTable (ids, offsets, DST rules).
page = ROOT / "tools/sync/hg_sync.html"
if page.is_file():
    html = page.read_text(encoding="utf-8")
    page_rows = re.findall(r'\["(\w+)", (-?\d+), "(\w*)"\]', html)
    fw_rows = [(i, off, "" if rule == "None" else rule) for i, off, rule in re.findall(
        r'\{"(\w+)", "[^"]*", "[^"]*", "[^"]*", "[^"]*", (-?\d+), DstRule::(\w+)\}',
        (SRC / "core/tz.cpp").read_text(encoding="utf-8"))]
    if not fw_rows or page_rows != fw_rows:
        errors.append(f"{rel(page)}:1: TZ_TABLE differs from kTzTable in src/core/tz.cpp")
    ids = {r[0] for r in fw_rows}
    for iana, zid in re.findall(r'"([A-Za-z_/]+)": "(\w+)"', html):
        if zid not in ids:
            errors.append(f"{rel(page)}:1: TZ_BY_IANA maps {iana} to unknown zone id {zid}")

for e in errors:
    print(e)
print(f"lint: {'FAILED, ' + str(len(errors)) + ' violation(s)' if errors else 'OK'} ({len(files)} files in src/)")
sys.exit(1 if errors else 0)
PY
