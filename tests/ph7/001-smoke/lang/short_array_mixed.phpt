--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: mixed with array() and in expressions
--FILE--
<?php
$a = array([1, 2], [3, 4]);
echo $a[0][0], "\n";
echo $a[1][1], "\n";

$b = [array(5, 6), array(7, 8)];
echo $b[0][1], "\n";
echo $b[1][0], "\n";

$c = [1, 2] + [2 => 3, 3 => 4];
echo count($c), "\n";
?>
--EXPECT--
1
4
6
7
4
--CLEAN--
<?php
unset($a,$b,$c);
