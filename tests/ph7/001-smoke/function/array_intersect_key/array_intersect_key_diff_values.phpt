--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key includes entries where keys match but values differ
--FILE--
<?php
$a = array("a" => "green", "b" => "brown");
$b = array("a" => "red", "b" => "yellow");
$r = array_intersect_key($a, $b);
echo count($r), PHP_EOL;
echo $r["a"], PHP_EOL;
echo $r["b"], PHP_EOL;
?>
--EXPECT--
2
green
brown
--CLEAN--
<?php
unset($a, $b, $r);
