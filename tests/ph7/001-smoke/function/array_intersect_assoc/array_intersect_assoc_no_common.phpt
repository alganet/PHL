--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc with no common key-value pairs returns empty array
--FILE--
<?php
$a = array("a" => 1, "b" => 2);
$b = array("c" => 1, "d" => 2);
$c = array_intersect_assoc($a, $b);
echo count($c);
echo PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $b, $c);
