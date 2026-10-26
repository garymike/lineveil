Lineveil (portable)
===================

A featherweight scanline overlay for the Windows desktop.

Run lineveil.exe. It starts in the system tray with the overlay on.

  Ctrl+Alt+F12         toggle the overlay
  Right-click tray     line style presets, edit config, exit

Portable mode
-------------
Because lineveil.ini sits next to lineveil.exe, Lineveil keeps all of its
settings in that file and writes nothing else to this computer: no registry,
no AppData. Copy the whole folder to a USB stick or a synced folder and your
settings travel with it. Delete the folder to remove it completely.

Edit lineveil.ini in any text editor; saved changes apply immediately.
Delete lineveil.ini and Lineveil switches to normal mode, keeping its
settings in %APPDATA%\lineveil instead.

Windows SmartScreen may warn on first run because the exe isn't code-signed
yet: choose More info, then Run anyway. To check the download is genuine,
compare its SHA-256 with SHA256SUMS.txt on the release page.

Website:  https://garymike.github.io/lineveil/
Source:   https://github.com/garymike/lineveil
License:  MIT (see LICENSE.txt)
