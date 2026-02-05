--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key should compare only keys for intersection
--FILE--
<?php
$a = array('x'=>1,'y'=>2,'z'=>3);
$b = array('y'=>9,'z'=>8);
$c = array_intersect_key($a,$b);
echo implode(',', array_keys($c)) . PHP_EOL; // y,z
?>
--EXPECT--
y,z
--CLEAN--
<?php
unset($a, $b, $c);
