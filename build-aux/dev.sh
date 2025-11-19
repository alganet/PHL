#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# cmd.exe environment wrapper for WSL environment
# ---
#
# Usage:
#   sh build-aux/dev.sh [wipe|run 'quoted cmd.exe command']
#
# Example:
#   sh build-aux/dev.sh run 'build-aux\dev.bat nmake /nologo /f Makefile'

# Get workspace directory
workspace_dir=$(cd "$(dirname "$0")/.." && pwd)

# Get Windows temp directory and convert to WSL path
win_temp="$(powershell.exe \
    -NoProfile -ExecutionPolicy Bypass \
    -Command 'Write-Host ${env:TEMP}')"
wsl_temp=$(wslpath -u "$win_temp")
tmpdir="$wsl_temp/PHL-build"


case "${1:-}" in
    "wipe")
        rm -rf "$tmpdir" build-aux/dev.cmd
        exit 0
        ;;
    "run")
        shift
        ;;
    *)
        echo "Usage: $0 [wipe|run 'quoted cmd.exe command']"
        exit 1
        ;;
esac

# Create temp build directory and copy source files
mkdir -p "$tmpdir"
rsync -a --delete "$workspace_dir/src" "$tmpdir" &
rsync -a --delete "$workspace_dir/tests" "$tmpdir" &
rsync -a --delete "$workspace_dir/build-aux" "$tmpdir" &
rsync -a "$workspace_dir/Makefile" "$tmpdir" &
wait

# Run build in temp directory via PowerShell
(timeout 60s powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
    Set-Location '$(wslpath -w "$tmpdir")';
    Start-Process -NoNewWindow -Wait -FilePath cmd.exe -ArgumentList '/c build-aux\dev.bat $*'")

# Copy build artifacts back to workspace
if test -d "$tmpdir/build/x86_64-windows-msvc"; then
    rsync -a --delete "$tmpdir/build/x86_64-windows-msvc/" "$workspace_dir/build/x86_64-windows-msvc/" &
fi

rsync -a "$tmpdir/build-aux" "$workspace_dir/" &
wait
