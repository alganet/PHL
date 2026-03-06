--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect with three arrays returns values common to all
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$b = array(2, 3, 4);
$c = array(3, 4, 6);
$r = array_intersect($a, $b, $c);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
2:3,3:4,
--CLEAN--
<?php
unset($a, $b, $c, $r);
