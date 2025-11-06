#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# NMake wrapper for WSL environment
# ---
#
# Usage:
#   sh build-aux/nmake.sh [nmake.exe arguments quoted]
#
# Example:
#   sh build-aux/nmake.sh /nologo /f 'build-aux\nmake.mk'

# Get workspace directory
workspace_dir=$(cd "$(dirname "$0")/.." && pwd)

# Get Windows temp directory and convert to WSL path
win_temp="$(powershell.exe \
    -NoProfile -ExecutionPolicy Bypass \
    -Command 'Write-Host ${env:TEMP}')"
wsl_temp=$(wslpath -u "$win_temp")
tmpdir="$wsl_temp/PH7-build"

# Create temp build directory and copy source files
mkdir -p "$tmpdir"
rsync -a "$workspace_dir/build-aux" "$tmpdir" &
rsync -a "$workspace_dir/src" "$tmpdir" &
rsync -a "$workspace_dir/examples" "$tmpdir" &
rsync -a "$workspace_dir/tests" "$tmpdir" &
wait

# Run build in temp directory via PowerShell
(timeout 60s powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
    Set-Location '$(wslpath -w "$tmpdir")';
    Start-Process -NoNewWindow -Wait -FilePath cmd.exe -ArgumentList '/c build-aux\nmake.bat $@'")

# Copy build artifacts back to workspace
if test -d "$tmpdir/build/x86_64-windows-msvc"; then
    rsync -a --delete "$tmpdir/build/x86_64-windows-msvc/" "$workspace_dir/build/x86_64-windows-msvc/" &
fi
rsync -a "$tmpdir/build-aux" "$workspace_dir/" &
wait
