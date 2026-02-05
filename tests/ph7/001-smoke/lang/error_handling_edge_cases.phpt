--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Basic operator coverage for uncovered lexer paths
--FILE--
<?php
echo "Testing basic operators:\n";

$a = 5; $b = 3;
echo ($a & $b) . "\n";  // Bitwise AND
echo ($a | $b) . "\n";  // Bitwise OR  
echo ($a ^ $b) . "\n";  // Bitwise XOR

echo ($a << 2) . "\n";  // Left shift
echo ($b >> 1) . "\n";  // Right shift

$a = 16; $a <<= 2; echo $a . "\n";  // Left shift assign
$b = 64; $b >>= 3; echo $b . "\n";  // Right shift assign

$a = 10; $a &= 5; echo $a . "\n";  // Bitwise AND assign
$b = 8; $b |= 2; echo $b . "\n";  // Bitwise OR assign
$c = 4; $c ^= 3; echo $c . "\n";  // Bitwise XOR assign

echo "1e10\n";  // Scientific notation
echo "2.5e-3\n";  // Scientific notation with negative exponent

echo "Test completed\n";
?>
--EXPECT--
Testing basic operators:
1
7
6
20
1
64
8
0
10
7
1e10
2.5e-3
Test completed
--CLEAN--
<?php
unset($a, $b, $c);
