--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice should return slices and optionally preserve keys
--FILE--
<?php
$a = array('a', 'b', 'c', 'd');
$s1 = array_slice($a, 1, 2);
echo implode(',', $s1) . PHP_EOL; // b,c
$s2 = array_slice($a, -2);
echo implode(',', $s2) . PHP_EOL; // c,d
$pres = array_slice($a, 1, 2, true);
echo implode(',', array_keys($pres)) . PHP_EOL; // 1,2
?>
--EXPECT--
b,c
c,d
1,2
--CLEAN--
<?php
unset($a,$s1,$s2,$pres);
?>
