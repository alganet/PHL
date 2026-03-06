--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc with three arrays intersects all
--FILE--
<?php
$a = array("a" => 1, "b" => 2, "c" => 3);
$b = array("a" => 1, "b" => 99, "c" => 3);
$d = array("a" => 1, "c" => 3, "d" => 4);
$c = array_intersect_assoc($a, $b, $d);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
a:1,c:3,
--CLEAN--
<?php
unset($a, $b, $d, $c);
