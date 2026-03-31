#!/bin/sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# Downloads FreeRTOS Kernel and FreeRTOS-Plus-TCP for the embedded port.
# Idempotent: skips download if dependencies are already present.

set -eu

FREERTOS_KERNEL_VERSION="V11.1.0"
FREERTOS_TCP_VERSION="V4.2.2"

DEPS_DIR="build/freertos-deps"
KERNEL_DIR="$DEPS_DIR/FreeRTOS-Kernel"
TCP_DIR="$DEPS_DIR/FreeRTOS-Plus-TCP"

KERNEL_URL="https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/${FREERTOS_KERNEL_VERSION}.tar.gz"
TCP_URL="https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/archive/refs/tags/${FREERTOS_TCP_VERSION}.tar.gz"

fetch_dep() {
    name="$1"
    url="$2"
    target="$3"

    if [ -d "$target" ]; then
        echo "$name: already present at $target"
        return
    fi

    echo "$name: downloading from $url"
    mkdir -p "$DEPS_DIR"
    tmpfile=$(mktemp)
    curl -fsSL "$url" -o "$tmpfile"
    mkdir -p "$target"
    tar xzf "$tmpfile" --strip-components=1 -C "$target"
    rm -f "$tmpfile"
    echo "$name: extracted to $target"
}

fetch_dep "FreeRTOS Kernel ${FREERTOS_KERNEL_VERSION}" "$KERNEL_URL" "$KERNEL_DIR"
fetch_dep "FreeRTOS-Plus-TCP ${FREERTOS_TCP_VERSION}" "$TCP_URL" "$TCP_DIR"

echo "All FreeRTOS dependencies ready in $DEPS_DIR"
