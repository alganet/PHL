--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
number_format with negative decimals rounds to tens/hundreds (php 8)
--FILE--
<?php
/* Negative $decimals rounds the value to tens/hundreds/... and prints no
 * fractional part -- number_format(1.5, -1) is '0', not '2'. */
echo number_format(1.5, -1), "\n";        // 0
echo number_format(12345.6, -2), "\n";    // 12,300
echo number_format(1234.5, -3), "\n";     // 1,000
echo number_format(5, -1), "\n";          // 10
echo number_format(-15.5, -1), "\n";      // -20
echo number_format(99.9, -2), "\n";       // 100
echo number_format(-49, -2), "\n";        // 0 (no "-0")
echo number_format(-0.4, -1), "\n";       // 0 (no "-0")
echo number_format(1000000, -2), "\n";    // 1,000,000
/* Regression: zero and positive decimals unchanged. */
echo number_format(1234.567, 0), "\n";    // 1,235
echo number_format(1234.567, 2), "\n";    // 1,234.57
echo number_format(-0.001, 2), "\n";      // 0.00 (no "-0.00")
?>
--EXPECT--
0
12,300
1,000
10
-20
100
0
0
1,000,000
1,235
1,234.57
0.00
