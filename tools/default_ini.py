"""Keeps res/lineveil.ini identical to the default config the app writes.

The app's defaults live in one place, the DEFAULT_INI string in src/config.c.
This script extracts that string so the portable zip ships exactly the same
file a normal first run creates.

    python3 tools/default_ini.py          # check (used by `make test`)
    python3 tools/default_ini.py --write  # regenerate res/lineveil.ini
"""
import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "config.c"
OUT = ROOT / "res" / "lineveil.ini"


def default_ini() -> bytes:
    text = SRC.read_text(encoding="utf-8")
    m = re.search(r"static const char DEFAULT_INI\[\]\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", text)
    if not m:
        sys.exit("DEFAULT_INI not found in src/config.c")
    # Adjacent C string literals concatenate; their escapes (\r \n \") match Python's.
    parts = re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1))
    return "".join(ast.literal_eval('"' + p + '"') for p in parts).encode("ascii")


def main() -> int:
    want = default_ini()
    if "--write" in sys.argv:
        OUT.write_bytes(want)
        print(f"wrote {OUT.relative_to(ROOT)} ({len(want)} bytes)")
        return 0
    have = OUT.read_bytes() if OUT.exists() else b""
    if have != want:
        print("res/lineveil.ini is out of date with DEFAULT_INI in src/config.c.")
        print("Run: python3 tools/default_ini.py --write")
        return 1
    print("res/lineveil.ini matches the app's defaults")
    return 0


if __name__ == "__main__":
    sys.exit(main())
