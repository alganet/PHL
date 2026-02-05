--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with a closure should transform values as expected and preserve keys
--FILE--
<?php
$a = array(10, 20, 30);
$b = array_map(function($v) { return $v * 2; }, $a);
echo implode(',', array_values($b)) . PHP_EOL; // '20,40,60'

// Also test associative input keeps keys and values are transformed
$assoc = array('x' => 1, 'y' => 2);
$assocMapped = array_map(function($v) { return $v + 1; }, $assoc);
echo implode(',', array_keys($assocMapped)) . PHP_EOL; // 'x,y'
echo implode(',', array_values($assocMapped)) . PHP_EOL; // '2,3'
?>
--EXPECT--
20,40,60
x,y
2,3
--CLEAN--
<?php
unset($a, $b, $assoc, $assocMapped);
