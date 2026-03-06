--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_assoc excludes entries with same key but different value
--FILE--
<?php
$a = array("x" => "hello");
$b = array("x" => "world");
$c = array_intersect_assoc($a, $b);
echo count($c);
echo PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $b, $c);
