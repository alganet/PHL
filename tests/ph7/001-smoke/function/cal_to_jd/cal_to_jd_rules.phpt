--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
cal_to_jd() screens each argument with its own wording
--FILE--
<?php
/* cal_to_jd() and cal_from_jd() are the four converters behind one calendar
 * id. Two things about the pair are only visible from outside: cal_to_jd()
 * takes its arguments in the (calendar, month, day, year) order the plain
 * converters use, and it screens each of them with a DIFFERENT wording -- the
 * day gets the full int range, the month a 1-based one, and the year only an
 * upper bound, so a year below the int range is narrowed rather than refused. */
function cal_to_jd_rules(): void {
    echo "## the same day through all four calendars\n";
    foreach ([[CAL_GREGORIAN, 1, 1, 1990], [CAL_JULIAN, 12, 19, 1989],
              [CAL_JEWISH, 4, 4, 5750], [CAL_FRENCH, 1, 1, 1]] as [$c, $m, $d, $y]) {
        printf("cal %d %2d/%2d/%-5d -> %d\n", $c, $m, $d, $y, cal_to_jd($c, $m, $d, $y));
    }
    echo "## a date the calendar has no room for is 0, not an exception\n";
    foreach ([[CAL_FRENCH, 1, 1, 15], [CAL_JEWISH, 1, 1, 0], [CAL_GREGORIAN, 1, 1, 0],
              [CAL_GREGORIAN, 1, 1, 2147483646], [CAL_GREGORIAN, 2147483646, 1, 2000],
              [CAL_GREGORIAN, 1, 1, PHP_INT_MIN]] as [$c, $m, $d, $y]) {
        printf("cal %d %d/%d/%d -> %d\n", $c, $m, $d, $y, cal_to_jd($c, $m, $d, $y));
    }
    echo "## four screens, four wordings\n";
    foreach ([[-1, 1, 1, 2000], [4, 1, 1, 2000], [0, 0, 1, 2000], [0, 2147483647, 1, 2000],
              [0, 1, 2147483648, 2000], [0, 1, -2147483649, 2000],
              [0, 1, 1, 2147483647]] as [$c, $m, $d, $y]) {
        try { cal_to_jd($c, $m, $d, $y); }
        catch (ValueError $e) { printf("%d,%d,%d,%d %s\n", $c, $m, $d, $y, $e->getMessage()); }
    }
    echo "## round trip through cal_from_jd for every calendar\n";
    foreach ([CAL_GREGORIAN, CAL_JULIAN, CAL_JEWISH, CAL_FRENCH] as $c) {
        $bad = 0; $n = 0;
        for ($jd = 2376000; $jd < 2380000; $jd += 3) {
            $r = cal_from_jd($jd, $c);
            if ($r['year'] === 0) { continue; }
            $n++;
            if (cal_to_jd($c, $r['month'], $r['day'], $r['year']) !== $jd) { $bad++; }
        }
        printf("cal %d: %d dates, %d mismatches\n", $c, $n, $bad);
    }
}
cal_to_jd_rules();
--EXPECT--
## the same day through all four calendars
cal 0  1/ 1/1990  -> 2447893
cal 1 12/19/1989  -> 2447893
cal 2  4/ 4/5750  -> 2447893
cal 3  1/ 1/1     -> 2375840
## a date the calendar has no room for is 0, not an exception
cal 3 1/1/15 -> 0
cal 2 1/1/0 -> 0
cal 0 1/1/0 -> 0
cal 0 1/1/2147483646 -> 0
cal 0 2147483646/1/2000 -> 0
cal 0 1/1/-9223372036854775808 -> 0
## four screens, four wordings
-1,1,1,2000 cal_to_jd(): Argument #1 ($calendar) must be a valid calendar ID
4,1,1,2000 cal_to_jd(): Argument #1 ($calendar) must be a valid calendar ID
0,0,1,2000 cal_to_jd(): Argument #2 ($month) must be between 1 and 2147483646
0,2147483647,1,2000 cal_to_jd(): Argument #2 ($month) must be between 1 and 2147483646
0,1,2147483648,2000 cal_to_jd(): Argument #3 ($day) must be between -2147483648 and 2147483647
0,1,-2147483649,2000 cal_to_jd(): Argument #3 ($day) must be between -2147483648 and 2147483647
0,1,1,2147483647 cal_to_jd(): Argument #4 ($year) must be less than 2147483646
## round trip through cal_from_jd for every calendar
cal 0: 1334 dates, 0 mismatches
cal 1: 1334 dates, 0 mismatches
cal 2: 1334 dates, 0 mismatches
cal 3: 1334 dates, 0 mismatches
