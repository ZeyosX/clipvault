#!/usr/bin/env bash
set -euo pipefail

install_prefix=${CLIPVAULT_INSTALL_PREFIX:-"$HOME/.local"}

if [[ $EUID -eq 0 ]]; then
    printf 'Run this uninstaller as your regular user.\n' >&2
    exit 1
fi
if [[ $install_prefix != /* || $install_prefix == / ]]; then
    printf 'CLIPVAULT_INSTALL_PREFIX must be an absolute directory other than /.\n' >&2
    exit 1
fi

rm -f -- "$install_prefix/bin/clipvault" \
    "$install_prefix/share/applications/clipvault.desktop" \
    "$install_prefix/share/icons/hicolor/scalable/apps/clipvault.svg"

if [[ $install_prefix == "$HOME/.local" ]]; then
    config_home=${XDG_CONFIG_HOME:-"$HOME/.config"}
    data_home=${XDG_DATA_HOME:-"$HOME/.local/share"}
    rm -f -- "$config_home/autostart/clipvault.desktop" \
        "$data_home/clipvault/clipvault.svg"
fi

if command -v update-desktop-database >/dev/null 2>&1 &&
   [[ -d "$install_prefix/share/applications" ]]; then
    update-desktop-database "$install_prefix/share/applications" || true
fi

printf 'Uninstalled ClipVault. Clipboard history and settings were kept.\n'
