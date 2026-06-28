--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable of a user function: f(...) wraps it in a callable Closure
--FILE--
<?php
function fcc_dbl($x){ return $x * 2; }
$f = fcc_dbl(...);
echo $f(21), "\n";
echo call_user_func($f, 5), "\n";
function fcc_sum(...$xs){ return array_sum($xs); }
$s = fcc_sum(...);
echo $s(4, 5, 6), "\n";
?>
--EXPECT--
42
10
15
--CLEAN--
<?php
