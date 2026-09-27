--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
juliantojd() keeps the Julian leap rule and its own epoch
--FILE--
<?php
/* The Julian calendar makes EVERY fourth year a leap year, centennial ones
 * included, which is the whole difference from the Gregorian. Its serial day
 * number 1 is 2 January 4713 B.C., one day later in the year than the
 * Gregorian's, so the two epochs' refusals sit at different dates. */
function cal_j2j(int $m, int $d, int $y): void {
    printf("%4d/%-3d/%-6d -> %d\n", $m, $d, $y, juliantojd($m, $d, $y));
}
function cal_juliantojd_rules(): void {
    echo "## the two calendars over the 1582 reform\n";
    cal_j2j(10, 4, 1582);
    cal_j2j(10, 5, 1582);
    printf("gregorian 10/15/1582 = %d\n", gregoriantojd(10, 15, 1582));

    echo "## 1900 is a leap year here and is not in the Gregorian\n";
    cal_j2j(2, 29, 1900);
    printf("gregorian 2/29/1900 -> %s\n", jdtogregorian(gregoriantojd(2, 29, 1900)));
    printf("julian    2/29/1900 -> %s\n", jdtojulian(juliantojd(2, 29, 1900)));

    echo "## the epoch, and the day it refuses\n";
    cal_j2j(1, 2, -4713);
    cal_j2j(1, 1, -4713);
    cal_j2j(12, 31, -4714);

    echo "## no year 0, no month or day outside the screen\n";
    cal_j2j(1, 1, 0);
    cal_j2j(1, 1, -1);
    cal_j2j(0, 1, 2000);
    cal_j2j(13, 1, 2000);
    cal_j2j(1, 0, 2000);
    cal_j2j(1, 32, 2000);
}
cal_juliantojd_rules();
--EXPECT--
## the two calendars over the 1582 reform
  10/4  /1582   -> 2299160
  10/5  /1582   -> 2299161
gregorian 10/15/1582 = 2299161
## 1900 is a leap year here and is not in the Gregorian
   2/29 /1900   -> 2415092
gregorian 2/29/1900 -> 3/1/1900
julian    2/29/1900 -> 2/29/1900
## the epoch, and the day it refuses
   1/2  /-4713  -> 1
   1/1  /-4713  -> 0
  12/31 /-4714  -> 0
## no year 0, no month or day outside the screen
   1/1  /0      -> 0
   1/1  /-1     -> 1721058
   0/1  /2000   -> 0
  13/1  /2000   -> 0
   1/0  /2000   -> 0
   1/32 /2000   -> 0
