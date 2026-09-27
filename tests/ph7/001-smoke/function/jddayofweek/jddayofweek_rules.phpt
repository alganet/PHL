--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jddayofweek() is arithmetic on the counter, no calendar
--FILE--
<?php
/* The weekday is `(sdn % 7 + 8) % 7` and nothing else -- no calendar is
 * consulted, so a serial day number every converter here rejects still has
 * one, and a negative one has a sensible answer too. Mode 1 is the long name,
 * mode 2 the short one, and every OTHER mode -- including a negative or an
 * unknown one -- falls through to the number. */
function jddayofweek_rules(): void {
    echo "## a week of days, counted from Sunday = 0\n";
    for ($jd = 2447892; $jd <= 2447898; $jd++) {
        printf("%d %d %-9s %s\n", $jd, jddayofweek($jd),
            jddayofweek($jd, CAL_DOW_LONG), jddayofweek($jd, CAL_DOW_SHORT));
    }
    echo "## every other mode is the day NUMBER\n";
    foreach ([-1, 0, 3, 99, PHP_INT_MAX, PHP_INT_MIN] as $mode) {
        printf("mode %-21d %s\n", $mode, var_export(jddayofweek(2447893, $mode), true));
    }
    echo "## no calendar is consulted, so any counter has a weekday\n";
    foreach ([0, -1, -7, -8, PHP_INT_MAX, PHP_INT_MIN] as $jd) {
        printf("%-21d %d %s\n", $jd, jddayofweek($jd), jddayofweek($jd, CAL_DOW_SHORT));
    }
}
jddayofweek_rules();
--EXPECT--
## a week of days, counted from Sunday = 0
2447892 0 Sunday    Sun
2447893 1 Monday    Mon
2447894 2 Tuesday   Tue
2447895 3 Wednesday Wed
2447896 4 Thursday  Thu
2447897 5 Friday    Fri
2447898 6 Saturday  Sat
## every other mode is the day NUMBER
mode -1                    1
mode 0                     1
mode 3                     1
mode 99                    1
mode 9223372036854775807   1
mode -9223372036854775808  1
## no calendar is consulted, so any counter has a weekday
0                     1 Mon
-1                    0 Sun
-7                    1 Mon
-8                    0 Sun
9223372036854775807   1 Mon
-9223372036854775808  0 Sun
