--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
abs is 64-bit-correct and returns a float for abs(PHP_INT_MIN)
--FILE--
<?php
// The integer path used a 32-bit C `int` + abs(), truncating any magnitude above
// 2^31 (abs(4294967296) was 0) and was UB on INT_MIN. It now reads the full
// 64-bit value; abs(PHP_INT_MIN) has no int representation so PHP returns a float.
echo abs(-5000000000), "\n";   // 5000000000 (> 2^32)
echo abs(4294967296), "\n";    // 4294967296 (> 2^31, was 0)
echo abs(-2147483648), "\n";   // 2147483648
echo abs(-3), "\n";            // 3 (small control)
$min = abs(PHP_INT_MIN);   // 2^63, no int representation -> float
echo is_float($min) ? "float " : "int ";
echo ($min == 9.223372036854776e18) ? "eq\n" : "ne\n";
?>
--EXPECT--
5000000000
4294967296
2147483648
3
float eq
--CLEAN--
<?php
