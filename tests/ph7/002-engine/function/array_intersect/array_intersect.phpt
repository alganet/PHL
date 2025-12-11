--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect returns items present in all arrays (value comparison)
--FILE--
<?php
$a = array(1,2,3,4);
$b = array(3,4,5);
$c = array_intersect($a, $b);
echo implode(',', array_values($c)) . PHP_EOL; // 3,4
?>
--EXPECT--
3,4
--CLEAN--
<?php
unset($a,$b,$c);
?>
