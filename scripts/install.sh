#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
install_prefix=${CLIPVAULT_INSTALL_PREFIX:-"$HOME/.local"}
build_dir=${CLIPVAULT_BUILD_DIR:-"$project_dir/build"}

if [[ $EUID -eq 0 ]]; then
    printf 'Run this installer as your regular user.\n' >&2
    exit 1
fi
if [[ $install_prefix != /* || $install_prefix == / ]]; then
    printf 'CLIPVAULT_INSTALL_PREFIX must be an absolute directory other than /.\n' >&2
    exit 1
fi

cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_prefix"
cmake --build "$build_dir" --parallel "${CLIPVAULT_BUILD_JOBS:-4}"
cmake --install "$build_dir"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$install_prefix/share/applications" || true
fi

printf 'Installed ClipVault in %s.\n' "$install_prefix"
if [[ $install_prefix == "$HOME/.local" ]]; then
    printf 'Find ClipVault in the Ubuntu app menu.\n'
fi
