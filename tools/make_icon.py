"""Generates res/lineveil.ico: an "L" monogram on a dark rounded tile.

Two drawings, chosen by size:
  - 40 px and up (Explorer, repo, site): the L is built from thin mint
    scanlines.
  - 32 px and below (tray, taskbar, title bar): a solid L with a faint rim.
    Lines that thin don't survive at tray sizes, and while the overlay is
    on, Lineveil draws its own scanlines over the tray icon anyway, so the
    icon gets "lined" live, using the user's own settings.

Each size is drawn at 4x and box-downsampled with every edge snapped to the
final pixel grid, so small sizes stay crisp. Entries are stored as PNG
(supported since Windows Vista).

    python3 tools/make_icon.py
"""
import io
import struct
from pathlib import Path

from PIL import Image, ImageDraw

TILE_TOP = (30, 35, 42)
TILE_BOTTOM = (12, 14, 18)
MINT = (126, 224, 181)        # lined L
MINT_BRIGHT = (170, 245, 212) # solid L: brighter to hold up at 16 px
SIZES = [16, 20, 24, 32, 40, 48, 64, 256]
SMALL_MAX = 32                # sizes at or below this get the solid L
SS = 4                        # supersampling factor


def _gradient(s):
    g = Image.new("RGBA", (s, s))
    d = ImageDraw.Draw(g)
    for y in range(s):
        t = y / (s - 1)
        c = tuple(round(a + (b - a) * t) for a, b in zip(TILE_TOP, TILE_BOTTOM))
        d.line([(0, y), (s, y)], fill=c + (255,))
    return g


def _radius(s):
    return max(SS * 3, round(s * 0.22))


def _clip(layer, s):
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, s - 1, s - 1), radius=_radius(s), fill=255)
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    img.paste(layer, (0, 0), mask)
    return img


def lined_L(size: int) -> Image.Image:
    """The L built from scanlines (the chosen design at large sizes)."""
    s = size * SS
    img = _gradient(s)
    d = ImageDraw.Draw(img)
    if size <= 48:
        period, thick = 4, 1
    elif size <= 64:
        period, thick = 5, 2
    else:
        period, thick = 12, 4
    m = max(3, round(size * 0.24))
    x0, x1, y0, y1 = m, size - m, m, size - m
    stem = max(2, round(size * 0.17))
    foot = max(2, round(size * 0.17))
    for y in range(y0, y1, period):
        right = x1 if y >= y1 - foot else x0 + stem
        d.rectangle((x0 * SS, y * SS, right * SS - 1, (y + thick) * SS - 1), fill=MINT + (255,))
    return _clip(img, s).reduce(SS)


def solid_L(size: int) -> Image.Image:
    """A solid L for tray sizes, with a faint rim for dark taskbars."""
    s = size * SS
    img = _gradient(s)
    d = ImageDraw.Draw(img)
    m = 3 if size <= 20 else round(size * 0.18)
    stem = 4 if size <= 20 else round(size * 0.19)
    foot = 3 if size <= 20 else round(size * 0.16)
    x0, x1, y0, y1 = m, size - m, m, size - m
    d.rectangle((x0 * SS, y0 * SS, (x0 + stem) * SS - 1, y1 * SS - 1), fill=MINT_BRIGHT + (255,))
    d.rectangle((x0 * SS, (y1 - foot) * SS, x1 * SS - 1, y1 * SS - 1), fill=MINT_BRIGHT + (255,))
    img = _clip(img, s)
    rim = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(rim).rounded_rectangle((0, 0, s - 1, s - 1), radius=_radius(s),
                                          outline=(255, 255, 255, 46), width=SS)
    return Image.alpha_composite(img, rim).reduce(SS)


def tile(size: int) -> Image.Image:
    return solid_L(size) if size <= SMALL_MAX else lined_L(size)


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
    (root / "build").mkdir(exist_ok=True)
    sheet = Image.new("RGBA", (sum(SIZES) + 10 * len(SIZES), 256 + 20), (240, 240, 240, 255))
    x = 10
    for im in imgs:
        sheet.alpha_composite(im, (x, 10))
        x += im.width + 10
    sheet.save(root / "build" / "icon-preview.png")
    print(f"wrote res/lineveil.ico ({(root / 'res' / 'lineveil.ico').stat().st_size} bytes)")
