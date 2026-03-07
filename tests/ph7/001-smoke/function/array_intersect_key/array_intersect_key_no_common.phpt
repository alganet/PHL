--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with no common keys returns empty array
--FILE--
<?php
$a = array("a" => 1, "b" => 2);
$b = array("c" => 3, "d" => 4);
$r = array_intersect_key($a, $b);
echo count($r), PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $b, $r);
