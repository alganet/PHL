#!/usr/bin/env sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# Script to extract per-file line coverage from lcov .info file
# Usage: ./per_file_coverage.sh <coverage.info>

INFO_FILE="$1"
if [ -z "$INFO_FILE" ]; then
    echo "Usage: $0 <coverage.info>"
    exit 1
fi

awk '
BEGIN { FS=":"; OFS=""; print "Filename                    Rate     Hit/Total" }
/^SF:/ { file = $2; gsub(/.*\/src\//, "src/", file) }
/^LF:/ { total = $2 }
/^LH:/ { hit = $2; if (total > 0) rate = sprintf("%.2f%%", (hit / total) * 100); else rate = "0.00%"; printf "%-30s %-8s (%d/%d)\n", file, rate, hit, total }
' "$INFO_FILE"