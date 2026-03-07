--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with single argument returns that array
--FILE--
<?php
$a = array("x" => 10, "y" => 20);
$r = array_intersect_key($a);
echo count($r), PHP_EOL;
echo $r["x"], PHP_EOL;
echo $r["y"], PHP_EOL;
?>
--EXPECT--
2
10
20
--CLEAN--
<?php
unset($a, $r);
