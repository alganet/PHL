--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
cal_days_in_month() subtracts two conversions, not a table
--FILE--
<?php
/* cal_days_in_month() has no month-length table at all: it converts the first
 * of the month and the first of the NEXT month and subtracts. That is what
 * makes it answer for a lunisolar calendar whose month lengths move year to
 * year -- and it is also why a month php cannot convert is a bare "Invalid
 * date" ValueError rather than a number. Two edges fall out of the same
 * arithmetic: the year after 1 B.C. is 1 A.D. and not 0, and the French
 * calendar's last month has to be given its end date by hand because there is
 * no year 15 to convert. */
function cal_dim(int $cal, int $m, int $y): void {
    try { printf("cal %d %2d/%-6d %d\n", $cal, $m, $y, cal_days_in_month($cal, $m, $y)); }
    catch (ValueError $e) { printf("cal %d %2d/%-6d %s\n", $cal, $m, $y, $e->getMessage()); }
}
function cal_days_in_month_rules(): void {
    echo "## the Gregorian and Julian leap rules disagree at 1900\n";
    cal_dim(CAL_GREGORIAN, 2, 1900);
    cal_dim(CAL_JULIAN, 2, 1900);
    cal_dim(CAL_GREGORIAN, 2, 2000);
    cal_dim(CAL_JULIAN, 2, 2000);

    echo "## December of 1 B.C. is followed by January of 1 A.D.\n";
    cal_dim(CAL_GREGORIAN, 12, -1);
    cal_dim(CAL_JULIAN, 12, -1);

    echo "## a Jewish year moves: Heshvan and Kislev are 29 or 30 days\n";
    foreach ([5784, 5785, 5786, 5787] as $y) {
        printf("year %d:", $y);
        for ($m = 1; $m <= 13; $m++) {
            try { printf(" %d", cal_days_in_month(CAL_JEWISH, $m, $y)); }
            catch (ValueError $e) { printf(" -"); }
        }
        echo "\n";
    }

    echo "## the French calendar, and the hand-written end of year 14\n";
    cal_dim(CAL_FRENCH, 12, 14);
    cal_dim(CAL_FRENCH, 13, 14);
    cal_dim(CAL_FRENCH, 13, 3);
    cal_dim(CAL_FRENCH, 1, 15);
    cal_dim(CAL_FRENCH, 1, 2000);

    echo "## a month the calendar cannot convert is a bare ValueError\n";
    cal_dim(CAL_GREGORIAN, 13, 2000);
    cal_dim(CAL_GREGORIAN, 1, 0);
    cal_dim(CAL_GREGORIAN, 1, -4714);
    cal_dim(CAL_JULIAN, 1, -4713);
    cal_dim(CAL_JEWISH, 1, -1);
    cal_dim(CAL_JEWISH, 14, 5786);

    echo "## the argument screens, each with its own wording\n";
    foreach ([[-1, 1, 2000], [4, 1, 2000], [0, 0, 2000], [0, 2147483647, 2000],
              [0, 1, 2147483647]] as [$c, $m, $y]) {
        try { cal_days_in_month($c, $m, $y); }
        catch (ValueError $e) { printf("%d,%d,%d %s\n", $c, $m, $y, $e->getMessage()); }
    }
    cal_dim(CAL_GREGORIAN, 2147483646, 2000);
}
cal_days_in_month_rules();
--EXPECT--
## the Gregorian and Julian leap rules disagree at 1900
cal 0  2/1900   28
cal 1  2/1900   29
cal 0  2/2000   29
cal 1  2/2000   29
## December of 1 B.C. is followed by January of 1 A.D.
cal 0 12/-1     31
cal 1 12/-1     31
## a Jewish year moves: Heshvan and Kislev are 29 or 30 days
year 5784: 30 29 29 29 30 30 29 30 29 30 29 30 29
year 5785: 30 30 30 29 30 0 29 30 29 30 29 30 29
year 5786: 30 29 30 29 30 0 29 30 29 30 29 30 29
year 5787: 30 30 30 29 30 30 29 30 29 30 29 30 29
## the French calendar, and the hand-written end of year 14
cal 3 12/14     30
cal 3 13/14     5
cal 3 13/3      6
cal 3  1/15     Invalid date
cal 3  1/2000   Invalid date
## a month the calendar cannot convert is a bare ValueError
cal 0 13/2000   Invalid date
cal 0  1/0      Invalid date
cal 0  1/-4714  Invalid date
cal 1  1/-4713  Invalid date
cal 2  1/-1     Invalid date
cal 2 14/5786   Invalid date
## the argument screens, each with its own wording
-1,1,2000 cal_days_in_month(): Argument #1 ($calendar) must be a valid calendar ID
4,1,2000 cal_days_in_month(): Argument #1 ($calendar) must be a valid calendar ID
0,0,2000 cal_days_in_month(): Argument #2 ($month) must be between 1 and 2147483646
0,2147483647,2000 cal_days_in_month(): Argument #2 ($month) must be between 1 and 2147483646
0,1,2147483647 cal_days_in_month(): Argument #3 ($year) must be less than 2147483646
cal 0 2147483646/2000   Invalid date
