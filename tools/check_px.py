"""check_px.py - count the raw pixel sizes left in guicy.h's widget, layout and tiling code.

PLAN-v2 F1 item 9 (report only in F1; D1 brings "unscaled" to zero). A "size" is:
  - an integer literal of 2 or more in arithmetic with a rect field (.x .y .w .h), or passed as an
    argument to a draw or layout helper (gy_canvas_*, gy__draw_icon, gy_canvas_box, gy_rect literals);
  - a #define whose name ends in _W, _H, _PAD, _GAP, _R or _SIZE.
Device-pixel constants that must stay (the 64 px dirty tile, 1024 atlas pages, 16384 frame limit)
carry a /*px*/ mark on their line and are counted apart.

    python tools/check_px.py            the counts, then each unscaled site (file:line: text)
    python tools/check_px.py --counts   the counts only
"""
import re, sys, os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(root, "guicy.h"), encoding="utf-8").read().split("\n")

# the sections that lay out and draw widgets: from the widgets banner to the renderer banner
start = next(i for i, l in enumerate(src) if "IDs and layout" in l)
end = next(i for i, l in enumerate(src) if "--- dirty tiles ---" in l)
# a size added to or taken from a rect field (multiplying a scaled size, or halving to centre, is
# not a size of its own)
rect_arith = re.compile(r"(\.[xywh]\b)\s*[-+]\s*([2-9]|\d\d+)\b(?!\s*\*)|(?<![*/] )\b([2-9]|\d\d+)\s*[-+]\s*\w*\.[xywh]\b")
helper_arg = re.compile(r"(gy_canvas_\w+|gy__draw_icon|gy_canvas_box|\(gy_rect\)\s*\{)[^;]*?[,({]\s*([2-9]|\d\d+)\s*[,)}]")
define_sz = re.compile(r"#define\s+\w*(_W|_H|_PAD|_GAP|_R|_SIZE)\s+\d")

unscaled, marked = [], []
for i in range(len(src)):
    line = src[i]
    in_code = start <= i < end
    hit = define_sz.search(line) or (in_code and (rect_arith.search(line) or helper_arg.search(line)))
    if not hit:
        continue
    (marked if "/*px*/" in line else unscaled).append((i + 1, line.strip()))

print(f"check_px: {len(unscaled)} unscaled pixel sizes, {len(marked)} marked /*px*/ (guicy.h lines {start + 1}-{end})")
if "--counts" not in sys.argv:
    for n, t in unscaled:
        print(f"guicy.h:{n}: {t[:110]}")
