--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
f(...) is a first-class callable; f(...$args) is still argument spread
--FILE--
<?php
function fcc_vs($a, $b, $c){ return "$a-$b-$c"; }
$args = [1, 2, 3];
echo fcc_vs(...$args), "\n";
$f = fcc_vs(...);
echo $f(4, 5, 6), "\n";
echo $f(...$args), "\n";
echo var_export($f instanceof Closure, true), "\n";
?>
--EXPECT--
1-2-3
4-5-6
1-2-3
true
--CLEAN--
<?php
