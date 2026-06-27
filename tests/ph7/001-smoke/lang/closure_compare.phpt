--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closures compare by identity (== and ===): distinct instances are never equal
--FILE--
<?php
function ccmp_mk(){ return function(){ return 42; }; }
$a = ccmp_mk(); $b = ccmp_mk(); $c = $a;
echo var_export($a == $b, true), "\n";   // distinct capture-less lambdas -> false
echo var_export($a === $b, true), "\n";  // false
echo var_export($a == $c, true), "\n";   // same instance -> true
echo var_export($a === $c, true), "\n";  // true
$x = fn() => 1; $y = fn() => 1;
echo var_export($x == $y, true), "\n";   // false
?>
--EXPECT--
false
false
true
true
false
--CLEAN--
<?php
