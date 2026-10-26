# scanlite

A featherweight scanline overlay for the Windows desktop.

- 12 to 14 KB executable, no C runtime, no dependencies beyond Windows system DLLs
- Runs on Windows 7 SP1 through 11, on x64, x86, and ARM64
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

**MSVC:** `build.bat [x64|x86|arm64]` (finds Visual Studio automatically, default x64).

**MinGW-w64** (MSYS2 or cross-compile from Linux/macOS): `make ARCH=x64|x86|arm64`. ARM64 needs [llvm-mingw](https://github.com/mstorsjo/llvm-mingw).

Output: `build\scanlite-<arch>.exe`

## Known limits

- Exclusive fullscreen games bypass DWM, so the overlay won't appear there. Borderless fullscreen works.
- UAC prompts (secure desktop) and some system UI can appear above it.
- DPI handling by Windows version: per-monitor v2 on Windows 10 1703+, per-monitor v1 on 8.1, system-aware on 7. On Windows 7, mixed-DPI setups aren't possible anyway.
- Hide-from-capture needs Windows 10 2004+. On older versions the option is ignored.
- HDR displays may render lines slightly differently than SDR. Not yet tested.

## Roadmap

- [ ] DirectComposition renderer (1 px strip, scaled with nearest-neighbor) and memory comparison vs GDI
- [ ] App icon resource
- [ ] Human-readable hotkey in config (`Ctrl+Alt+F12`)
- [ ] Per-monitor settings
- [ ] Code signing
- [ ] macOS and Linux ports

## License

MIT
