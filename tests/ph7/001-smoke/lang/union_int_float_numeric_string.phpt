--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union int|float: numeric-int strings coerce to int, float-shaped strings coerce to float
--FILE--
<?php
function uifns_f(int|float $x) {
    echo is_int($x) ? "int:" : "flt:", $x, "\n";
}
uifns_f("42");      // numeric int -> int
uifns_f("-7");      // numeric int (negative) -> int
uifns_f("3.14");    // numeric float -> float
uifns_f("-2.5");    // negative float -> float
uifns_f("  5  ");   // PHP allows surrounding whitespace; integer-shaped -> int
?>
--EXPECT--
int:42
int:-7
flt:3.14
flt:-2.5
int:5
--CLEAN--
<?php
