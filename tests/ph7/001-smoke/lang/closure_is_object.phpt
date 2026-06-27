--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous functions and arrow functions are Closure objects (instanceof / gettype)
--FILE--
<?php
$a = function($x){ return $x; };
$b = fn($x) => $x;
$c = function() use ($a) { return $a; };
echo var_export($a instanceof Closure, true), "\n";
echo var_export($b instanceof Closure, true), "\n";
echo var_export($c instanceof Closure, true), "\n";
echo gettype($a), "\n";
echo var_export(is_string($a), true), "\n";
echo get_class($b), "\n";
?>
--EXPECT--
true
true
true
object
false
Closure
--CLEAN--
<?php
