--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jewishtojd() walks the metonic cycle, and screens its year
--FILE--
<?php
/* The Jewish calendar is lunisolar: a 19-year metonic cycle decides which
 * years get a thirteenth month, and four "dehiyyot" rules can push the start
 * of a year up to two days later than its new moon. That is why a year can be
 * 353, 354, 355, 383, 384 or 385 days long -- and why only Kislev and Heshvan
 * need the year's length to be placed at all. */
function cal_e2j(int $m, int $d, int $y): void {
    printf("%3d/%-3d/%-6d -> %d\n", $m, $d, $y, jewishtojd($m, $d, $y));
}
function cal_jewishtojd_rules(): void {
    echo "## the start of the calendar\n";
    cal_e2j(1, 1, 1);
    cal_e2j(1, 1, 0);
    cal_e2j(1, 1, -1);

    echo "## the six year lengths, measured from Tishri 1 to Tishri 1\n";
    $seen = [];
    for ($y = 5700; $y < 5800; $y++) {
        $seen[jewishtojd(1, 1, $y + 1) - jewishtojd(1, 1, $y)] = true;
    }
    ksort($seen);
    printf("lengths: %s\n", implode(' ', array_keys($seen)));

    echo "## a leap year has Adar I and Adar II, a regular one does not\n";
    foreach ([5784, 5785, 5786] as $y) {
        printf("%d has %d months, month 6 starts at %d\n",
            $y, jewishtojd(1, 1, $y + 1) - jewishtojd(1, 1, $y) > 360 ? 13 : 12,
            jewishtojd(6, 1, $y));
    }

    echo "## the screens: day 1-30, month 1-13, year past 0\n";
    cal_e2j(1, 0, 5786);
    cal_e2j(1, 31, 5786);
    cal_e2j(0, 1, 5786);
    cal_e2j(14, 1, 5786);

    echo "## the year is range-checked, not narrowed like the other three --\n";
    echo "## and php keeps the first day of the year in an int, so it WRAPS\n";
    foreach ([2147483645, 2147483646, 2147483647] as $y) {
        printf("%d -> %d\n", $y, jewishtojd(1, 1, $y));
    }
    foreach ([2147483648, -2147483649, PHP_INT_MAX, PHP_INT_MIN] as $y) {
        try { jewishtojd(1, 1, $y); }
        catch (ValueError $e) { printf("%-21d %s\n", $y, $e->getMessage()); }
    }

    echo "## round trip over three thousand years\n";
    $bad = 0;
    for ($jd = 1400000; $jd < 2500000; $jd += 11) {
        [$m, $d, $y] = array_map('intval', explode('/', jdtojewish($jd)));
        if (jewishtojd($m, $d, $y) !== $jd) { $bad++; }
    }
    printf("mismatches: %d\n", $bad);
}
cal_jewishtojd_rules();
--EXPECT--
## the start of the calendar
  1/1  /1      -> 347998
  1/1  /0      -> 0
  1/1  /-1     -> 0
## the six year lengths, measured from Tishri 1 to Tishri 1
lengths: 353 354 355 383 384 385
## a leap year has Adar I and Adar II, a regular one does not
5784 has 13 months, month 6 starts at 2460351
5785 has 12 months, month 6 starts at 2460736
5786 has 12 months, month 6 starts at 2461090
## the screens: day 1-30, month 1-13, year past 0
  1/0  /5786   -> 0
  1/31 /5786   -> 0
  0/1  /5786   -> 0
 14/1  /5786   -> 0
## the year is range-checked, not narrowed like the other three --
## and php keeps the first day of the year in an int, so it WRAPS
2147483645 -> -1617090478
2147483646 -> 0
2147483647 -> 0
2147483648            jewishtojd(): Argument #3 ($year) must be between -2147483648 and 2147483647
-2147483649           jewishtojd(): Argument #3 ($year) must be between -2147483648 and 2147483647
9223372036854775807   jewishtojd(): Argument #3 ($year) must be between -2147483648 and 2147483647
-9223372036854775808  jewishtojd(): Argument #3 ($year) must be between -2147483648 and 2147483647
## round trip over three thousand years
mismatches: 0
