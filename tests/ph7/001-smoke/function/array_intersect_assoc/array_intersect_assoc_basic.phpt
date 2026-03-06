--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc returns values with matching keys and values
--FILE--
<?php
$a = array("a" => "green", "b" => "brown", "c" => "blue", "red");
$b = array("a" => "green", "b" => "yellow", "blue", "red");
$c = array_intersect_assoc($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
a:green,
--CLEAN--
<?php
unset($a, $b, $c);
