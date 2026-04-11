--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union weak coercion: int|float on numeric strings
--FILE--
<?php
function uwcp_f(int|float $x) {
    echo is_int($x) ? "int:" : "flt:", $x, "\n";
}
uwcp_f(1);       // exact int
uwcp_f(1.5);     // exact float
uwcp_f("3");     // numeric int -> int
uwcp_f("3.14");  // numeric float -> float
?>
--EXPECT--
int:1
flt:1.5
int:3
flt:3.14
--CLEAN--
<?php
