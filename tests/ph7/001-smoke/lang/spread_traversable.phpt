--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Traversable spread: array-literal [...$it] and call-argument f(...$it)
--FILE--
<?php
function spGen() { yield 1; yield 2; yield 3; }
// Array-literal spread of a Traversable (int keys renumbered).
echo implode(",", [0, ...spGen(), 9]), "\n";
// String keys are preserved (PHP 8.1).
function spKv() { yield "a" => 1; yield "b" => 2; }
$kv = [...spKv()];
echo $kv["a"], $kv["b"], "\n";
// Call-argument spread of a Traversable held in a variable.
function spSum(...$a) { return array_sum($a); }
$g = spGen();
echo spSum(...$g), "\n";
// A non-Traversable object still raises the spread error.
class SpPlain {}
try { $x = [...new SpPlain()]; }
catch (\Throwable $e) { echo get_class($e), "\n"; }
?>
--EXPECT--
0,1,2,3,9
12
6
TypeError
--CLEAN--
<?php
unset($kv, $g);
