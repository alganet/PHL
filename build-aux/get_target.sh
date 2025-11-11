#!/bin/sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

arch=$(uname -m)
os=$(uname -s)

if [ "$os" = "Linux" ]; then
    echo "${arch}-linux-gnu"
elif [ "$os" = "Darwin" ]; then
    echo "${arch}-apple-darwin"
else
    echo "${arch}-unknown"
fi