--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with several arrays applies the callback in parallel
--DESCRIPTION--
Regression: array_map ignored every array after the first, calling the callback
with a single argument. PHP walks the arrays in parallel, passing one element
from each per iteration and re-indexing the result with sequential integer keys.
--FILE--
<?php
$a = [1, 2, 3];
$b = [10, 20, 30];
$c = [100, 200, 300];
$r = array_map(function ($x, $y) { return $x + $y; }, $a, $b);
echo $r[0], ',', $r[1], ',', $r[2], PHP_EOL;
$s = array_map(function ($x, $y, $z) { return $x + $y + $z; }, $a, $b, $c);
echo $s[0], ',', $s[1], ',', $s[2], PHP_EOL;
?>
--EXPECT--
11,22,33
111,222,333
--CLEAN--
<?php
unset($a, $b, $c, $r, $s);
