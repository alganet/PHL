--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with string keys returns matching entries
--FILE--
<?php
$a = array("a" => 1, "b" => 2, "c" => 3);
$b = array("b" => 99, "c" => 88, "d" => 77);
$r = array_intersect_key($a, $b);
echo count($r), PHP_EOL;
echo $r["b"], PHP_EOL;
echo $r["c"], PHP_EOL;
echo isset($r["a"]) ? '1' : '0', PHP_EOL;
?>
--EXPECT--
2
2
3
0
--CLEAN--
<?php
unset($a, $b, $r);
