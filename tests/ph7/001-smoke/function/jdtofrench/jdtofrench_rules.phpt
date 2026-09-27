--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdtofrench() answers 0/0/0 for every day outside the calendar
--FILE--
<?php
/* Unlike the other three converters, this one has a hard window: the day
 * before the calendar started and the day after it ended are both "0/0/0". */
function cal_jdtofrench_rules(): void {
    foreach ([2375839, 2375840, 2375841, 2380952, 2380953, 0, -1, 2440588, PHP_INT_MAX] as $jd) {
        printf("%-21d %s\n", $jd, jdtofrench($jd));
    }
    echo "## the holidays at the end of a year, month 13\n";
    foreach ([2376204, 2376205, 2376206, 2377300, 2377301] as $jd) {
        printf("%d %-8s %s\n", $jd, jdtofrench($jd), jdtogregorian($jd));
    }
}
cal_jdtofrench_rules();
--EXPECT--
2375839               0/0/0
2375840               1/1/1
2375841               1/2/1
2380952               13/5/14
2380953               0/0/0
0                     0/0/0
-1                    0/0/0
2440588               0/0/0
9223372036854775807   0/0/0
## the holidays at the end of a year, month 13
2376204 13/5/1   9/21/1793
2376205 1/1/2    9/22/1793
2376206 1/2/2    9/23/1793
2377300 13/5/4   9/21/1796
2377301 1/1/5    9/22/1796
