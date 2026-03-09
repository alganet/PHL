#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# cmd.exe environment wrapper for WSL environment
# ---
#
# Usage:
#   sh build-aux/dev.sh wipe|run|make [OPTIONS]
#
# Example:
#   sh build-aux/dev.sh run 'build-aux\dev.bat nmake test' # always quote
#   sh build-aux/dev.sh make test # equivalent to above

# Get workspace directory
workspace_dir=$(cd "$(dirname "$0")/.." && pwd)

# Get Windows temp directory and convert to WSL path
# Uses cmd.exe instead of powershell.exe to avoid WSL interop stdin hangs
# (PowerShell's .NET runtime can block on inherited stdin during cold start)
win_temp="$(cmd.exe /c 'echo %TEMP%' < /dev/null 2>/dev/null | tr -d '\r')"
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
    "make")
        shift
        set -- 'build-aux\dev.bat nmake /nologo' "$@"
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
rsync -a --delete "$workspace_dir/examples" "$tmpdir" &
cp "$workspace_dir/Makefile" "$tmpdir/Makefile" &
wait

# Ensure we have the structure to rsync back
mkdir -p "$workspace_dir/build/x86_64-windows-msvc"

# Run build in temp directory via cmd.exe directly (avoids powershell.exe stdin hangs)
# Subshell cd to $tmpdir so cmd.exe inherits a Windows-native working directory
# (prevents "UNC paths are not supported" warning from \\wsl.localhost\... paths)
(cd "$tmpdir" && cmd.exe /c "$*" < /dev/null)

# Copy build artifacts back to workspace
if test -d "$tmpdir/build/x86_64-windows-msvc"; then
    rsync -a --delete "$tmpdir/build/x86_64-windows-msvc/" "$workspace_dir/build/x86_64-windows-msvc/"
fi
