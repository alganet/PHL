--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key with empty first array returns empty array
--FILE--
<?php
$a = array();
$b = array("a" => 1, "b" => 2);
$r = array_intersect_key($a, $b);
echo count($r), PHP_EOL;
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a, $b, $r);
