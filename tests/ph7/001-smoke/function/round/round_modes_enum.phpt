--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
round()'s third argument is php 8.4's RoundingMode enum, beside the integer spelling
--DESCRIPTION--
php declares round()'s $mode as `RoundingMode|int`, and the two numberings do NOT
agree past the four HALF_* modes: integer 5 is CEILING while the enum's fifth case
is TowardsZero, so a case cannot be mapped by its ordinal. Every case is paired
here with the integer that must answer the same thing. The refusals are the other
half: a non-numeric string satisfies neither arm (no coercion makes an instance),
which php reports against the whole declared type rather than as an invalid mode.
--FILE--
<?php
function round_modes_enum_probe(): void {
    $vals = [2.5, 3.5, -2.5, 1.4, -1.4];
    /* case name => the integer mode that must agree with it */
    $pairs = [
        'HalfAwayFromZero' => 1, 'HalfTowardsZero' => 2,
        'HalfEven' => 3, 'HalfOdd' => 4,
        'TowardsZero' => 7, 'AwayFromZero' => 8,
        'NegativeInfinity' => 6, 'PositiveInfinity' => 5,
    ];
    foreach ($pairs as $name => $int) {
        $case = constant("RoundingMode::$name");
        $byCase = $byInt = [];
        foreach ($vals as $v) {
            $byCase[] = round($v, 0, $case);
            $byInt[] = round($v, 0, $int);
        }
        printf("%-17s int=%d %s agree=%s\n", $name, $int,
            implode(' ', array_map(fn($f) => var_export($f, true), $byCase)),
            var_export($byCase === $byInt, true));
    }
    echo "## precision still applies, and the enum is the DEFAULT mode's spelling\n";
    var_dump(round(1.2345, 2, RoundingMode::HalfEven));
    var_dump(round(1.2355, 3, RoundingMode::HalfAwayFromZero) === round(1.2355, 3));
    var_dump(round(-1234.5, -2, RoundingMode::PositiveInfinity));
    echo "## the refusals\n";
    foreach ([9, 0, -1, PHP_INT_MAX] as $bad) {
        try { round(1.5, 0, $bad); } catch (Throwable $e) {
            printf("%-20d %s: %s\n", $bad, get_class($e), $e->getMessage());
        }
    }
    foreach (['x', 'HalfEven', ''] as $bad) {
        try { round(1.5, 0, $bad); } catch (Throwable $e) {
            printf("%-20s %s: %s\n", var_export($bad, true), get_class($e), $e->getMessage());
        }
    }
    try { round(1.5, 0, new stdClass()); } catch (Throwable $e) {
        printf("%-20s %s: %s\n", 'stdClass', get_class($e), $e->getMessage());
    }
    echo "## a NUMERIC string still reaches the int arm, weak-mode\n";
    var_dump(round(1.5, 0, "3"), round(1.5, 0, " 3"), round(1.5, 0, "3.0"));
}
round_modes_enum_probe();
?>
--EXPECT--
HalfAwayFromZero  int=1 3.0 4.0 -3.0 1.0 -1.0 agree=true
HalfTowardsZero   int=2 2.0 3.0 -2.0 1.0 -1.0 agree=true
HalfEven          int=3 2.0 4.0 -2.0 1.0 -1.0 agree=true
HalfOdd           int=4 3.0 3.0 -3.0 1.0 -1.0 agree=true
TowardsZero       int=7 2.0 3.0 -2.0 1.0 -1.0 agree=true
AwayFromZero      int=8 3.0 4.0 -3.0 2.0 -2.0 agree=true
NegativeInfinity  int=6 2.0 3.0 -3.0 1.0 -2.0 agree=true
PositiveInfinity  int=5 3.0 4.0 -2.0 2.0 -1.0 agree=true
## precision still applies, and the enum is the DEFAULT mode's spelling
float(1.23)
bool(true)
float(-1200)
## the refusals
9                    ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
0                    ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
-1                   ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
9223372036854775807  ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
'x'                  TypeError: round(): Argument #3 ($mode) must be of type RoundingMode|int, string given
'HalfEven'           TypeError: round(): Argument #3 ($mode) must be of type RoundingMode|int, string given
''                   TypeError: round(): Argument #3 ($mode) must be of type RoundingMode|int, string given
stdClass             TypeError: round(): Argument #3 ($mode) must be of type RoundingMode|int, stdClass given
## a NUMERIC string still reaches the int arm, weak-mode
float(2)
float(2)
float(2)
