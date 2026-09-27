--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdtogregorian() answers month/day/year, and 0/0/0 off the scale
--FILE--
<?php
/* The inverse of gregoriantojd(). An SDN it cannot place -- zero, a negative
 * one, or one past the arithmetic's own guards -- is the string "0/0/0" rather
 * than an exception, and the year steps straight from -1 to 1. */
function cal_jdtogregorian_rules(): void {
    foreach ([1, 2, 1721425, 1721426, 2299160, 2299161, 2440588, 2447893, 5373484] as $jd) {
        printf("%-10d %s\n", $jd, jdtogregorian($jd));
    }
    echo "## the failure answer\n";
    foreach ([0, -1, -5, PHP_INT_MAX, PHP_INT_MIN, intdiv(PHP_INT_MAX, 4)] as $jd) {
        printf("%-21d %s\n", $jd, jdtogregorian($jd));
    }
    echo "## round trip over four centuries\n";
    $bad = 0;
    for ($jd = 2200000; $jd < 2500000; $jd += 7) {
        [$m, $d, $y] = array_map('intval', explode('/', jdtogregorian($jd)));
        if (gregoriantojd($m, $d, $y) !== $jd) { $bad++; }
    }
    printf("mismatches: %d\n", $bad);
}
cal_jdtogregorian_rules();
--EXPECT--
1          11/25/-4714
2          11/26/-4714
1721425    12/31/-1
1721426    1/1/1
2299160    10/14/1582
2299161    10/15/1582
2440588    1/1/1970
2447893    1/1/1990
5373484    12/31/9999
## the failure answer
0                     0/0/0
-1                    0/0/0
-5                    0/0/0
9223372036854775807   0/0/0
-9223372036854775808  0/0/0
2305843009213693951   0/0/0
## round trip over four centuries
mismatches: 0
