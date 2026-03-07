--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with three arrays intersects all keys
--FILE--
<?php
$a = array("a" => 1, "b" => 2, "c" => 3, "d" => 4);
$b = array("a" => 10, "b" => 20, "e" => 50);
$c = array("b" => 100, "c" => 300, "a" => 200);
$r = array_intersect_key($a, $b, $c);
echo count($r), PHP_EOL;
echo $r["a"], PHP_EOL;
echo $r["b"], PHP_EOL;
echo isset($r["c"]) ? '1' : '0', PHP_EOL;
echo isset($r["d"]) ? '1' : '0', PHP_EOL;
?>
--EXPECT--
2
1
2
0
0
--CLEAN--
<?php
unset($a, $b, $c, $r);
