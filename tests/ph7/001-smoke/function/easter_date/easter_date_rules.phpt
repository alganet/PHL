--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
easter_date() is the same Sunday, built with the C library
--FILE--
<?php
/* easter_date() is easter_days() turned into a timestamp -- midnight at the
 * start of Easter Sunday. Unlike everything else in this extension it touches
 * the CLOCK: php builds the date with the C library's mktime(), which reads
 * the PROCESS timezone rather than date.timezone, so the timestamps move with
 * the box's zone and the assertions below are relational. The timestamp form
 * also narrows the year twice more than easter_days() does: there is no
 * timestamp before 1970, and php stops at the year two billion. */
function easter_date_rules(): void {
    echo "## it lands on exactly the day easter_days() names\n";
    $bad = 0;
    for ($y = 1970; $y < 2200; $y++) {
        $d = easter_days($y);
        $m = $d < 11 ? 3 : 4;
        $day = $d < 11 ? $d + 21 : $d - 10;
        /* unixtojd() breaks the timestamp back down through the same
         * localtime(), so the pair round-trips in any zone. */
        if (unixtojd(easter_date($y)) !== gregoriantojd($m, $day, $y)) { $bad++; }
    }
    printf("disagreements with easter_days(): %d\n", $bad);

    echo "## a decade of Sundays, named through the counter\n";
    for ($y = 2020; $y < 2030; $y++) {
        $jd = unixtojd(easter_date($y));
        printf("%d %-10s %s %s\n", $y, jdtogregorian($jd),
            jddayofweek($jd, CAL_DOW_LONG), jdmonthname($jd, CAL_MONTH_GREGORIAN_LONG));
    }

    echo "## the four modes, where they disagree\n";
    foreach ([1970, 2000, 2026] as $y) {
        printf("%d %s %s %s %s\n", $y,
            jdtogregorian(unixtojd(easter_date($y, CAL_EASTER_DEFAULT))),
            jdtogregorian(unixtojd(easter_date($y, CAL_EASTER_ROMAN))),
            jdtogregorian(unixtojd(easter_date($y, CAL_EASTER_ALWAYS_GREGORIAN))),
            jdtogregorian(unixtojd(easter_date($y, CAL_EASTER_ALWAYS_JULIAN))));
    }

    echo "## the timestamp form's own two screens\n";
    foreach ([1969, 1, 2000000001, 7378697629483820644] as $y) {
        try { easter_date($y); }
        catch (ValueError $e) { printf("%-21d %s\n", $y, $e->getMessage()); }
    }
    var_dump(is_int(easter_date(1970)), is_int(easter_date(2000000000)));

    echo "## and easter_days()'s own screen still runs first\n";
    foreach ([0, -1, PHP_INT_MAX] as $y) {
        try { easter_date($y); }
        catch (ValueError $e) { printf("%-21d %s\n", $y, $e->getMessage()); }
    }
    echo "## no argument is the current year\n";
    var_dump(easter_date() === easter_date((int)date('Y')),
             easter_date(null) === easter_date((int)date('Y')));
}
easter_date_rules();
--EXPECT--
## it lands on exactly the day easter_days() names
disagreements with easter_days(): 0
## a decade of Sundays, named through the counter
2020 4/12/2020  Sunday April
2021 4/4/2021   Sunday April
2022 4/17/2022  Sunday April
2023 4/9/2023   Sunday April
2024 3/31/2024  Sunday March
2025 4/20/2025  Sunday April
2026 4/5/2026   Sunday April
2027 3/28/2027  Sunday March
2028 4/16/2028  Sunday April
2029 4/1/2029   Sunday April
## the four modes, where they disagree
1970 3/29/1970 3/29/1970 3/29/1970 4/13/1970
2000 4/23/2000 4/23/2000 4/23/2000 4/17/2000
2026 4/5/2026 4/5/2026 4/5/2026 3/30/2026
## the timestamp form's own two screens
1969                  easter_date(): Argument #1 ($year) must be a year after 1970 (inclusive)
1                     easter_date(): Argument #1 ($year) must be a year after 1970 (inclusive)
2000000001            easter_date(): Argument #1 ($year) must be a year before 2.000.000.000 (inclusive)
7378697629483820644   easter_date(): Argument #1 ($year) must be a year before 2.000.000.000 (inclusive)
bool(true)
bool(true)
## and easter_days()'s own screen still runs first
0                     easter_date(): Argument #1 ($year) must be between 1 and 7378697629483820644
-1                    easter_date(): Argument #1 ($year) must be between 1 and 7378697629483820644
9223372036854775807   easter_date(): Argument #1 ($year) must be between 1 and 7378697629483820644
## no argument is the current year
bool(true)
bool(true)
