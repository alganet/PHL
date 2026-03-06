--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect returns values present in both arrays
--FILE--
<?php
$a = array(1, 2, 3, 4);
$b = array(3, 4, 5);
$c = array_intersect($a, $b);
foreach ($c as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
2:3,3:4,
--CLEAN--
<?php
unset($a, $b, $c);
