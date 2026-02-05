--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should use the user callback to compute intersections by value
--FILE--
<?php
$a = array(0 => 'a', 1 => 'b', 2 => 'c');
$b = array(0 => 'c', 1 => 'd', 2 => 'a');
$c = array_uintersect($a, $b, function($x, $y) { return strcmp($x, $y); });
// Expect 2 entries, keys preserved from $a
echo count($c) . PHP_EOL;
foreach($c as $k => $v) {
    echo $k . ':' . $v . PHP_EOL;
}
?>
--EXPECT--
2
0:a
2:c
--CLEAN--
<?php
unset($a, $b, $c);
