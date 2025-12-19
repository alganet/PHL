--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with deeply nested arrays and COUNT_RECURSIVE
--FILE--
<?php
// Create deeply nested array structure
$a = array(1);
$b = array($a);
$c = array($b);
$d = array($c);
$e = array($d);
$f = array($e);
$g = array($f);
$h = array($g);
$i = array($h);
$j = array($i);
$k = array($j);

// Count recursively
$count = count($k, COUNT_RECURSIVE);
echo "Recursive count: $count\n";

// Expected: 1 (top level) + 1 (nested) + 1 (value) = 3, but wait:
// $k = array($j) -> 1 element
// $j = array($i) -> 1 element
// ...
// $a = array(1) -> 1 element
// So total: 10 arrays + 1 value = 11
echo "Expected around 11\n";
?>
--EXPECT--
Recursive count: 11
Expected around 11