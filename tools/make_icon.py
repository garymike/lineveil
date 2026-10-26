"""Generates res/lineveil.ico: a glowing rounded tile veiled by dark scanlines.

Each size is drawn separately at 4x and box-downsampled, with every line
snapped to the final pixel grid so small sizes stay crisp. Sizes are stored
as PNG entries (supported since Windows Vista).

    python3 tools/make_icon.py
"""
import io
import struct
from pathlib import Path

from PIL import Image, ImageDraw

GLOW_TOP = (150, 238, 200)     # light mint
GLOW_BOTTOM = (34, 128, 104)   # deep teal
LINE = (10, 14, 18)            # near-black scanlines
SIZES = [16, 20, 24, 32, 40, 48, 64, 256]
SS = 4                         # supersampling factor


def tile(size: int) -> Image.Image:
    """A glowing rounded 'screen' with dark scanlines running edge to edge."""
    s = size * SS

    # Vertical glow gradient.
    grad = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / (s - 1)
        c = tuple(round(a + (b - a) * t) for a, b in zip(GLOW_TOP, GLOW_BOTTOM))
        gd.line([(0, y), (s, y)], fill=c + (255,))

    # Scanlines snapped to the final pixel grid, full bleed. Drawn on their
    # own layer and composited so they darken the glow instead of punching
    # see-through holes in the icon.
    if size <= 24:
        period, thick = 3, 1
    elif size <= 48:
        period, thick = 4, 1
    elif size <= 64:
        period, thick = 5, 2
    else:
        period, thick = 12, 4
    lines = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ld = ImageDraw.Draw(lines)
    offset = ((size - thick) % period) // 2
    for y in range(offset, size, period):
        ld.rectangle((0, y * SS, s - 1, (y + thick) * SS - 1), fill=LINE + (170,))
    grad = Image.alpha_composite(grad, lines)

    # Rounded tile mask.
    mask = Image.new("L", (s, s), 0)
    radius = max(SS * 3, round(s * 0.22))
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, s - 1, s - 1), radius=radius, fill=255)

    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    img.paste(grad, (0, 0), mask)
    return img.reduce(SS)  # box filter keeps snapped edges sharp


def write_ico(images, path: Path) -> None:
    entries, blobs = [], []
    offset = 6 + 16 * len(images)
    for im in images:
        buf = io.BytesIO()
        im.save(buf, format="PNG", optimize=True)
        data = buf.getvalue()
        w, h = im.size
        entries.append(struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(data), offset))
        blobs.append(data)
        offset += len(data)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(images)))
        f.writelines(entries)
        f.writelines(blobs)


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    imgs = [tile(n) for n in SIZES]
    write_ico(imgs, root / "res" / "lineveil.ico")
    # Preview sheet for review (not committed).
    sheet = Image.new("RGBA", (sum(SIZES) + 10 * len(SIZES), 256 + 20), (240, 240, 240, 255))
    x = 10
    for im in imgs:
        sheet.alpha_composite(im, (x, 10))
        x += im.width + 10
    sheet.save(root / "build" / "icon-preview.png")
    print(f"wrote res/lineveil.ico ({(root / 'res' / 'lineveil.ico').stat().st_size} bytes)")
