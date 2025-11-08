--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Basic arithmetic operations
--FILE--
<?php
$a = 10;
$b = 3;
echo $a + $b; // 13
echo "\n";
echo $a - $b; // 7
echo "\n";
echo $a * $b; // 30
echo "\n";
echo $a / $b; // 3.333...
echo "\n";
echo $a % $b; // 1
?>
--EXPECTF--
13
7
30
3.333333333333%d
1
--CLEAN--
<?php
unset($a, $b);
?>