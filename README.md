# ClipVault (Ubuntu Noble Clipboard History) — C++ / Qt6 / SQLite

ClipVault is a small tray app that records clipboard history (text + images) to SQLite and shows a popup history window.
Default hotkey: **Ctrl+F1**.

## Features implemented
- Clipboard monitoring (text + images)
- SQLite-backed history with max-entry pruning
- Tray icon with menu (Show History / Settings / Quit)
- Popup window closes on focus loss
- Multi-select (Shift + ↑/↓) and paste selected entries on Enter
- Settings:
  - Hotkey string (default Ctrl+F1)

## Extra UX features
- Pin/favorites (P)
- Delete selected entries (Del)
- Preview panel (text/image)
- Type-to-filter without losing arrow-key focus

## Settings:
  - Max entries in history
  - Popup "Always on top"
  - Start on login (autostart .desktop)

## Wayland vs Xorg
- **Global shortcut (Ctrl+F1)**:
  - Wayland: uses the **XDG GlobalShortcuts portal** when available.
  - Xorg/X11: uses **XGrabKey**.
- **Auto paste on Enter**:
  - Wayland: uses the **XDG RemoteDesktop portal** (requests KEYBOARD control once).
  - Xorg/X11: uses **XTest** to synthesize Ctrl+V.

If you don’t want RemoteDesktop permissions on Wayland, you can disable "Auto paste" in settings; Enter will copy the selection to the clipboard and close the popup.

## Dependencies (Ubuntu 24.04 Noble)
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-base-dev-tools \
  libsqlite3-dev \
  libx11-dev libxtst-dev
```

> Tray icon note (GNOME): Ubuntu typically ships AppIndicator support as a system extension.
> If you don't see the tray icon, install/enable AppIndicator support (package varies by Ubuntu version).

## Build & Run
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/clipvault
```

## Install on Ubuntu

From the project directory, run:

```bash
./scripts/install.sh
```

This builds ClipVault and installs it for your user in `~/.local/bin`, with an icon and a launcher in `~/.local/share`. Open **ClipVault** from Ubuntu's app menu. A normal launch opens the history window; `clipvault --hidden` starts in the tray only. Opening ClipVault from the menu again brings up the running instance.

To uninstall:

```bash
./scripts/uninstall.sh
```

The uninstaller removes the app, launcher, icon, and login autostart entry. It keeps your history database and settings. The build dependencies above are needed for installation; runtime Qt and SQLite libraries must remain installed afterward.

## Project layout

- `src/main.cpp`, `src/app.*`: entry point and application wiring
- `src/config/`: settings and login autostart
- `src/history/`: clipboard capture and SQLite storage
- `src/ui/`: history window, list model, and tray icon
- `src/integration/`: Wayland portal and X11 shortcuts and paste support
- `src/util/`: shared helpers
- `assets/`: icon, Qt resource manifest, and desktop launcher
- `scripts/`: per-user installer and uninstaller

## Files created
- DB: `~/.local/share/clipvault/history.db`
- Settings: `~/.config/clipvault/settings.ini`
- Autostart: `~/.config/autostart/clipvault.desktop`

## Keyboard shortcuts in popup
- ↑/↓ : move
- Shift + ↑/↓ : extend selection
- Enter : paste (or copy-only if Auto paste is disabled)
- P : toggle pin
- Del : delete selected
- Ctrl+Backspace : clear filter
- Esc : close

## DB schema
Table `entries` holds either:
- text rows (type=0, text filled), or
- image rows (type=1, image_png blob filled)
