--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
easter_days() picks its rule from the year, not just the mode
--FILE--
<?php
/* easter_days() is the number of days AFTER 21 March that Easter falls on, and
 * it is pure arithmetic -- no clock, no timezone. Its $mode decides which
 * calendar's rule is used, and the default is not one rule but a DATE-DEPENDENT
 * choice: Julian up to 1582, Julian again for 1583-1752 (England kept the old
 * calendar that long), Gregorian after. CAL_EASTER_ROMAN moves the 1583-1752
 * window to the Gregorian rule; the two ALWAYS_ modes pin one rule for every
 * year. */
function easter_days_rules(): void {
    echo "## the default rule changes twice, at 1582 and at 1752\n";
    foreach ([1582, 1583, 1700, 1752, 1753, 1800, 2026] as $y) {
        printf("%-5d default=%-2d roman=%-2d greg=%-2d julian=%d\n", $y,
            easter_days($y, CAL_EASTER_DEFAULT), easter_days($y, CAL_EASTER_ROMAN),
            easter_days($y, CAL_EASTER_ALWAYS_GREGORIAN),
            easter_days($y, CAL_EASTER_ALWAYS_JULIAN));
    }
    echo "## a decade, and the March/April date each answer means\n";
    for ($y = 2020; $y < 2030; $y++) {
        $d = easter_days($y);
        printf("%d %2d %s\n", $y, $d,
            $d < 11 ? sprintf('March %d', $d + 21) : sprintf('April %d', $d - 10));
    }
    echo "## an unknown mode is the default rule\n";
    foreach ([-1, 4, 99, PHP_INT_MAX, PHP_INT_MIN] as $mode) {
        printf("mode %-21d %d\n", $mode, easter_days(2026, $mode));
    }
    echo "## the year screen, and the ARITHMETIC at the top of it: the limit is\n";
    echo "## LONG_MAX/5*4, which is exactly the headroom the Gregorian rule needs\n";
    echo "## -- the Julian one adds 1.25*year and overflows for the last two\n";
    foreach ([1, 7378697629483820642, 7378697629483820643, 7378697629483820644] as $y) {
        printf("%d -> default=%d", $y, easter_days($y));
        /* what the overflowed Julian arithmetic answers is the compiler's (x86-64
         * and arm64 php disagree), so only the years it fits are pinned */
        if ($y < 7378697629483820643) {
            printf(" julian=%d", easter_days($y, CAL_EASTER_ALWAYS_JULIAN));
        }
        echo "\n";
    }
    foreach ([0, -1, 7378697629483820645, PHP_INT_MAX] as $y) {
        try { easter_days($y); }
        catch (ValueError $e) { printf("%-21d %s\n", $y, $e->getMessage()); }
    }
    echo "## easter is always between 22 March and 25 April\n";
    $lo = 99; $hi = -1;
    for ($y = 1600; $y < 2400; $y++) {
        $d = easter_days($y);
        if ($d < $lo) { $lo = $d; }
        if ($d > $hi) { $hi = $d; }
    }
    printf("min=%d max=%d\n", $lo, $hi);
}
easter_days_rules();
--EXPECT--
## the default rule changes twice, at 1582 and at 1752
1582  default=25 roman=25 greg=28 julian=25
1583  default=10 roman=20 greg=20 julian=10
1700  default=10 roman=21 greg=21 julian=10
1752  default=8  roman=12 greg=12 julian=8
1753  default=32 roman=32 greg=32 julian=21
1800  default=23 roman=23 greg=23 julian=18
2026  default=15 roman=15 greg=15 julian=9
## a decade, and the March/April date each answer means
2020 22 April 12
2021 14 April 4
2022 27 April 17
2023 19 April 9
2024 10 March 31
2025 30 April 20
2026 15 April 5
2027  7 March 28
2028 26 April 16
2029 11 April 1
## an unknown mode is the default rule
mode -1                    15
mode 4                     15
mode 99                    15
mode 9223372036854775807   15
mode -9223372036854775808  15
## the year screen, and the ARITHMETIC at the top of it: the limit is
## LONG_MAX/5*4, which is exactly the headroom the Gregorian rule needs
## -- the Julian one adds 1.25*year and overflows for the last two
1 -> default=6 julian=6
7378697629483820642 -> default=27 julian=26
7378697629483820643 -> default=19
7378697629483820644 -> default=3
0                     easter_days(): Argument #1 ($year) must be between 1 and 7378697629483820644
-1                    easter_days(): Argument #1 ($year) must be between 1 and 7378697629483820644
7378697629483820645   easter_days(): Argument #1 ($year) must be between 1 and 7378697629483820644
9223372036854775807   easter_days(): Argument #1 ($year) must be between 1 and 7378697629483820644
## easter is always between 22 March and 25 April
min=1 max=35
