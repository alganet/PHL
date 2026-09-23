--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
escapeshellarg() refuses what QUOTING made too long for the command line
--SKIPIF--
skip: flaky
--FILE--
<?php
/* A POSIX `'` costs four bytes once quoted (`'\''`), so an argument well inside
 * the limit can leave it after escaping. The limit is the platform's, so it is
 * read back out of the first refusal rather than assumed. */
$max = 0;
try {
    escapeshellarg(str_repeat('a', 3000000));
    echo "NO THROW (arg)\n";
} catch (ValueError $e) {
    if (preg_match('/(\d+)/', $e->getMessage(), $m)) { $max = (int)$m[1]; }
}
$half = intdiv($max, 2) + 1;
try {
    escapeshellarg(str_repeat("'", $half));
    echo "NO THROW (arg escaped)\n";
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
echo "still here\n";
?>
--EXPECTF--
Escaped argument exceeds the allowed length of %d bytes
still here
