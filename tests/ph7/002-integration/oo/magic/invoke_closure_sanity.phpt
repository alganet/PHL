--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure invocation is unaffected by the __invoke object-callable path
--FILE--
<?php
$f = function ($a, $b) { return $a * $b; };
echo $f(3, 4), "\n";

$arrow = fn($x) => $x + 1;
echo $arrow(41), "\n";

$captured = 100;
$closure = function ($x) use ($captured) { return $x + $captured; };
echo $closure(5), "\n";
?>
--EXPECT--
12
42
105
--CLEAN--
<?php
