--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should use the user callback to compute differences by value
--FILE--
<?php
$a = array(0 => 1, 1 => 2, 2 => 3);
$b = array(0 => 3, 1 => 4);
$c = array_udiff($a, $b, function($x, $y) { return $x - $y; });
// Expected: values 1 and 2 remain
echo count($c) . PHP_EOL;
foreach($c as $k => $v) { echo $k . ':' . $v . PHP_EOL; }
?>
--EXPECT--
2
0:1
1:2
--CLEAN--
<?php
unset($a,$b,$c);
?>
