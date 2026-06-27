--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure type-hints (param, return, property) accept closures
--FILE--
<?php
function cth_take(Closure $c){ return $c(10); }
echo cth_take(fn($x) => $x * 2), "\n";
echo cth_take(function($x){ return $x + 1; }), "\n";
function cth_make(): Closure { return fn() => "ret"; }
$m = cth_make(); echo $m(), "\n";
class CthBox { public Closure $fn; }
$b = new CthBox(); $b->fn = fn() => "prop";
$f = $b->fn; echo $f(), "\n";
?>
--EXPECT--
20
11
ret
prop
--CLEAN--
<?php
