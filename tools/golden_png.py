"""Make PNG copies of gytest's pictures for people to look at.

    python tools/golden_png.py            work/out/*.bmp -> tests/golden/*.png
    python tools/golden_png.py --sheet    also one contact sheet, work/out/sheet.png

The hashes in tests/golden/hashes.txt are what the tests check; the PNGs are only for eyes.
Needs Pillow (X:/tools/python315 has it).
"""
import glob, os, sys
from PIL import Image

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(root, "work", "out")
dst = os.path.join(root, "tests", "golden")
os.makedirs(dst, exist_ok=True)
ims = []
for f in sorted(glob.glob(os.path.join(src, "*.bmp"))):
    im = Image.open(f).convert("RGB")
    name = os.path.splitext(os.path.basename(f))[0]
    im.save(os.path.join(dst, name + ".png"), optimize=True)
    ims.append((name, im))
    print("png", name, im.size)
if "--sheet" in sys.argv and ims:
    w = max(i.size[0] for _, i in ims)
    h = sum(i.size[1] + 4 for _, i in ims)
    sheet = Image.new("RGB", (w, h), (90, 90, 90))
    y = 0
    for _, i in ims:
        sheet.paste(i, (0, y)); y += i.size[1] + 4
    sheet.save(os.path.join(src, "sheet.png"))
    print("sheet", sheet.size)
