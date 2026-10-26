# scanlite

A featherweight scanline overlay for the Windows desktop.

- ~14 KB executable, no C runtime, no dependencies beyond Windows system DLLs
- Zero CPU at idle: the pattern is drawn once and composited by DWM
- No screen capture: the app never reads your screen, so no recording permission is involved
- Click-through, always on top, one overlay per monitor, per-monitor DPI aware
- Tray icon and global hotkey (default `Ctrl+Alt+F12`)
- Plain INI config, hot-reloaded on save
- Optional: hide the overlay from screenshots and screen sharing

## How it works

Each monitor gets a borderless, layered, click-through, topmost window. A 32-bit premultiplied pattern is rendered once and handed to DWM with `UpdateLayeredWindow`, then freed. After that the process sits blocked in `MsgWaitForMultipleObjects` until something changes (hotkey, tray, display change, config save).

## Config

On first run `%APPDATA%\scanlite\scanlite.ini` is created with defaults. For portable use, put a `scanlite.ini` next to the exe instead.

| Key | Default | Notes |
| --- | --- | --- |
| `enabled` | `1` | Overlay on at startup |
| `period` | `3` | Pixels from one line to the next (2 to 64) |
| `thickness` | `1` | Line thickness (1 to period minus 1) |
| `opacity` | `80` | 0 to 255 |
| `color` | `000000` | RRGGBB hex |
| `orientation` | `horizontal` | or `vertical` |
| `exclude_from_capture` | `0` | Hide from screenshots and screen share (Win10 2004+) |
| `renderer` | `gdi` | `dcomp` is planned, falls back to `gdi` for now |
| `hotkey_mods` | `3` | Alt=1, Ctrl=2, Shift=4, Win=8 (sum them) |
| `hotkey_vk` | `123` | Decimal virtual-key code (123 = F12) |

Tray menu toggles are runtime only. The INI holds startup state.

## Build

**MSVC:** run `build.bat` (finds Visual Studio automatically).

**MinGW-w64** (MSYS2 or cross-compile from Linux/macOS): `make`

Output: `build\scanlite.exe`

## Known limits

- Exclusive fullscreen games bypass DWM, so the overlay won't appear there. Borderless fullscreen works.
- UAC prompts (secure desktop) and some system UI can appear above it.
- Requires Windows 10 1703 or later.

## Roadmap

- [ ] DirectComposition renderer (1 px strip, scaled with nearest-neighbor) and memory comparison vs GDI
- [ ] App icon resource
- [ ] Human-readable hotkey in config (`Ctrl+Alt+F12`)
- [ ] Per-monitor settings
- [ ] Code signing
- [ ] macOS and Linux ports

## License

MIT
