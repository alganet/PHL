--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdtojulian() answers month/day/year, and 0/0/0 off the scale
--FILE--
<?php
function cal_jdtojulian_rules(): void {
    foreach ([1, 2, 1721424, 2299160, 2299161, 2440588, 2447893] as $jd) {
        printf("%-10d %s\n", $jd, jdtojulian($jd));
    }
    echo "## the failure answer\n";
    foreach ([0, -1, PHP_INT_MAX, PHP_INT_MIN] as $jd) {
        printf("%-21d %s\n", $jd, jdtojulian($jd));
    }
    echo "## round trip over four centuries\n";
    $bad = 0;
    for ($jd = 2200000; $jd < 2500000; $jd += 7) {
        [$m, $d, $y] = array_map('intval', explode('/', jdtojulian($jd)));
        if (juliantojd($m, $d, $y) !== $jd) { $bad++; }
    }
    printf("mismatches: %d\n", $bad);
}
cal_jdtojulian_rules();
--EXPECT--
1          1/2/-4713
2          1/3/-4713
1721424    1/1/1
2299160    10/4/1582
2299161    10/5/1582
2440588    12/19/1969
2447893    12/19/1989
## the failure answer
0                     0/0/0
-1                    0/0/0
9223372036854775807   0/0/0
-9223372036854775808  0/0/0
## round trip over four centuries
mismatches: 0
