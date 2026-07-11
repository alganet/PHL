--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dechex prints the full 64-bit two's-complement for negatives
--FILE--
<?php
// PHP casts $num to a 64-bit int and prints its unsigned representation, so a
// negative value spans all 16 hex digits (not a 32-bit-truncated one).
echo dechex(-1) . "\n";           // ffffffffffffffff
echo dechex(-255) . "\n";         // ffffffffffffff01
echo dechex(PHP_INT_MIN) . "\n";  // 8000000000000000
echo dechex(4294967296) . "\n";   // 100000000 (> 2^32, not truncated to 0)
echo dechex(255) . "\n";          // ff  (positive control)
?>
--EXPECT--
ffffffffffffffff
ffffffffffffff01
8000000000000000
100000000
ff
--CLEAN--
<?php
