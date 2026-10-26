# Lineveil

A featherweight scanline overlay for the Windows desktop.

**[Live preview and config generator](https://garymike.github.io/lineveil/)** · **[Download](https://github.com/garymike/lineveil/releases/latest)**

- ~30 KB executable (icon included), no C/C++ runtime, imports only four core Windows DLLs
- Runs on Windows 7 SP1 through 11, on x64, x86, and ARM64
- Zero CPU at idle: the pattern is drawn once and composited by Windows
- No screen capture: the app never reads your screen, so no recording permission is involved
- Click-through, always on top, one overlay per monitor, per-monitor DPI aware
- Tray icon and global hotkey (default `Ctrl+Alt+F12`)
- Plain INI config, hot-reloaded on save
- Optional: hide the overlay from screenshots and screen sharing

## How it works

Each monitor gets a borderless, layered, click-through, topmost window. The line pattern is rendered once and handed to the Desktop Window Manager (DWM), then the process sits blocked in `MsgWaitForMultipleObjects` until something changes (hotkey, tray, display change, config save).

Two renderers:

- **`gdi`** (default): a full-size premultiplied bitmap via `UpdateLayeredWindow`. The app frees its copy immediately; DWM keeps one (about 33 MB per 4K monitor). Works everywhere.
- **`dcomp`** (Windows 8+, experimental): a 1-pixel strip stretched across the monitor by DirectComposition with nearest-neighbor sampling. A few KB of surface instead of megabytes, but it loads the GPU driver into the process, so whether it's lighter overall depends on your system. `d3d11.dll` and `dcomp.dll` are loaded only when this renderer is selected. Falls back to `gdi` automatically if it can't start.

## Config

On first run `%APPDATA%\lineveil\lineveil.ini` is created with defaults. For portable use, put a `lineveil.ini` next to the exe instead.

| Key | Default | Notes |
| --- | --- | --- |
| `enabled` | `1` | Overlay on at startup |
| `preset` | (empty) | `224p`, `240p`, `288p` or `480`. See [Presets](#presets). |
| `lines` | (empty) | Custom line count per screen (16 to 2160). Overrides `preset`. |
| `fill` | `50` | With `preset` or `lines`: percent of each line cycle that's dark (10 to 90) |
| `period` | `3` | Without a preset: pixels from one line to the next (2 to 64) |
| `thickness` | `1` | Without a preset: line thickness (1 to period minus 1) |
| `opacity` | `80` | 0 to 255 |
| `color` | `000000` | RRGGBB hex |
| `orientation` | `horizontal` | or `vertical` |
| `exclude_from_capture` | `0` | Hide from screenshots and screen share (Win10 2004+) |
| `renderer` | `gdi` | or `dcomp` (see above) |
| `hotkey` | `Ctrl+Alt+F12` | Modifiers `Ctrl` `Alt` `Shift` `Win`; keys `A-Z`, `0-9`, `F1-F24`, `Num0-Num9`, `Space`, `Tab`, `Enter`, `Esc`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, arrows, `Pause`, `ScrollLock`. `None` disables it. |

Invalid values fall back to defaults. Tray menu toggles are runtime only; the INI holds startup state.

## Presets

Real scanlines came from the source resolution, not the screen: a 240p console drew 240 lines whether the TV was 13 or 32 inches. Presets work the same way. They set a **line count per monitor**, so every display shows the same number of lines whatever its resolution, and mixed setups (a 4K next to a 1080p) match.

| Preset | Lines | Based on | Spacing at 1080p | 1440p | 4K |
| --- | --- | --- | --- | --- | --- |
| `224p` | 224 | Most arcade boards, much of SNES | 4.8 px | 6.4 | 9.6 |
| `240p` | 240 | NES, Genesis, PS1 and other NTSC consoles | 4.5 | 6 | 9 |
| `288p` | 288 | PAL consoles | 3.75 | 5 | 7.5 |
| `480` | 480 | VGA 640×480, 480i/480p video | 2.25 | 3 | 4.5 |

Line counts are active lines per frame (per field for interlaced systems). How dark and thick the lines are has no standard; it varied with every CRT's beam and phosphors, so `fill` and `opacity` stay yours to tune. When the spacing isn't a whole number of pixels, cycles alternate (4 and 5 px for 240p at 1080p) while the total count stays exact.

Switch presets live from the tray's **Line style** menu.

## Verifying a download

Each release includes `SHA256SUMS.txt` and a signed [build provenance attestation](https://docs.github.com/actions/security-for-github-actions/using-artifact-attestations) proving the exe was built by this repo's release workflow:

```
gh attestation verify lineveil-v0.1.0-windows-x64.exe --repo garymike/lineveil
```

Binaries are not code-signed yet, so SmartScreen may warn on first run.

## Build

**MSVC:** `build.bat [x64|x86|arm64]` (finds Visual Studio automatically, default x64).

**MinGW-w64** (MSYS2 or cross-compile from Linux/macOS): `make ARCH=x64|x86|arm64`. ARM64 needs [llvm-mingw](https://github.com/mstorsjo/llvm-mingw).

Output: `build\lineveil-<arch>.exe`

**Tests** (Linux/macOS, no Windows needed): `make test` runs unit tests for the hotkey parser and the line pattern under ASan and UBSan, including a sweep confirming the exact line count on every screen size from 100 to 4320 px. `make fuzz` fuzzes it with libFuzzer (needs clang).

**Icon:** `res/lineveil.ico` is generated by `make icon` (Python 3 + Pillow) and committed.

## Releasing

1. Bump `res/version.h` (and the version in `res/lineveil.manifest`).
2. Tag and push: `git tag v0.1.0 && git push origin v0.1.0`
3. The release workflow builds all three architectures with MSVC, checks the tag matches `version.h`, writes checksums, attests provenance, and opens a **draft** release to review and publish.

Actions are pinned to commit SHAs and kept current by Dependabot.

## Known limits

- Exclusive fullscreen games bypass DWM, so the overlay won't appear there. Borderless fullscreen works.
- UAC prompts (secure desktop) and some system UI can appear above it.
- DPI handling by Windows version: per-monitor v2 on Windows 10 1703+, per-monitor v1 on 8.1, system-aware on 7.
- Hide-from-capture needs Windows 10 2004+. On older versions the option is ignored.
- HDR displays may render lines slightly differently than SDR. Not yet tested.
- With `dcomp`, a GPU driver reset can blank the overlay until the next display change or toggle.

## Roadmap

- [x] Human-readable hotkey
- [x] App icon, manifest, version info
- [x] DirectComposition renderer
- [ ] Measure `gdi` vs `dcomp` memory on real hardware
- [x] Resolution-independent presets
- [ ] Per-monitor settings
- [ ] Code signing
- [ ] macOS and Linux ports

## Website

The project page is `docs/index.html`, served by GitHub Pages from `main` / `docs`. It's one self-contained file with a strict CSP and no external requests.

## License

MIT
