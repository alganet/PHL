--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
frenchtojd() covers the fourteen years the calendar ran
--FILE--
<?php
/* The French republican calendar ran from 22 September 1792 and was abandoned
 * in January 1806, and the conversion package refuses everything outside years
 * 1 to 14 rather than extrapolating a leap rule nobody ever settled. Twelve
 * months of thirty days, then a thirteenth "month" of five or six holidays. */
function cal_f2j(int $m, int $d, int $y): void {
    printf("%3d/%-3d/%-4d -> %d\n", $m, $d, $y, frenchtojd($m, $d, $y));
}
function cal_frenchtojd_rules(): void {
    echo "## the first and last days the calendar has\n";
    cal_f2j(1, 1, 1);
    cal_f2j(13, 5, 14);
    printf("%s\n", jdtofrench(frenchtojd(1, 1, 1)));
    printf("gregorian: %s\n", jdtogregorian(frenchtojd(1, 1, 1)));

    echo "## outside the fourteen years\n";
    cal_f2j(1, 1, 0);
    cal_f2j(1, 1, 15);
    cal_f2j(1, 1, -1);

    echo "## the thirteenth month, and the screens on month and day\n";
    cal_f2j(13, 1, 3);
    cal_f2j(13, 6, 3);
    cal_f2j(14, 1, 3);
    cal_f2j(0, 1, 3);
    cal_f2j(1, 0, 3);
    cal_f2j(1, 31, 3);

    echo "## round trip across the whole calendar\n";
    $bad = 0;
    for ($jd = 2375840; $jd <= 2380952; $jd++) {
        [$m, $d, $y] = array_map('intval', explode('/', jdtofrench($jd)));
        if (frenchtojd($m, $d, $y) !== $jd) { $bad++; }
    }
    printf("mismatches: %d\n", $bad);
}
cal_frenchtojd_rules();
--EXPECT--
## the first and last days the calendar has
  1/1  /1    -> 2375840
 13/5  /14   -> 2380952
1/1/1
gregorian: 9/22/1792
## outside the fourteen years
  1/1  /0    -> 0
  1/1  /15   -> 0
  1/1  /-1   -> 0
## the thirteenth month, and the screens on month and day
 13/1  /3    -> 2376930
 13/6  /3    -> 2376935
 14/1  /3    -> 0
  0/1  /3    -> 0
  1/0  /3    -> 0
  1/31 /3    -> 0
## round trip across the whole calendar
mismatches: 0
