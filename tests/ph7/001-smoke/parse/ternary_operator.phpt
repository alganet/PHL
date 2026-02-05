--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test ternary operator expressions
--FILE--
<?php
// Test ternary operator expressions to cover conditional expressions
$a = 5;
$result = $a > 3 ? $a * 2 : $a + 10;
echo $result; // Should be 10
echo "\n";

$b = 2;
$result2 = $b < 1 ? $b - 5 : ($b > 0 ? $b + 3 : $b);
echo $result2; // Should be 5
echo "\n";

$c = 0;
$result3 = $c ? 1 : 0;
echo $result3; // Should be 0
echo "\n";
?>
--EXPECT--
10
5
0
--CLEAN--
<?php
unset($a, $result, $b, $result2, $c, $result3);
