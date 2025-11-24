--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 only: strglob matches patterns
--SKIPIF--
<?php
if (!function_exists('strglob')) {
    // Skip on PHP runtimes
    echo "skip strglob not available\n";
}
?>
--FILE--
<?php
// Basic match
echo strglob('*.txt', 'readme.txt') ? "1\n" : "0\n";
// Mismatch
echo strglob('*.php', 'index.html') ? "1\n" : "0\n";
?>
--EXPECT--
1
0
