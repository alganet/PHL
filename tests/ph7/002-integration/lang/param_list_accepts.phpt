--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trailing comma after a variadic parameter, and $This, are ordinary parameters
--FILE--
<?php
function ok(int $a, string ...$rest,) { return count($rest); }
echo ok(1, "a", "b"), "\n";
$f = fn($x, ...$y) => count($y);
echo $f(1, 2, 3), "\n";
class K { function m($This, $a = 1, ...$c) { return $This + $a + count($c); } }
echo (new K)->m(7), "\n";
echo (new K)->m(7, 2, 3, 4), "\n";
?>
--EXPECT--
2
2
8
11
--CLEAN--
<?php
