--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Lexer scientific notation and combined operators
--FILE--
<?php
// Test scientific notation edge cases
echo "Scientific notation tests:\n";

// Basic scientific notation
echo 1e10 . "\n";
echo 2.5e-3 . "\n";
echo 3.14e+2 . "\n";

// Scientific notation in expressions
$result = 1e5 * 2.5e-2;
echo $result . "\n";

// Test combined assignment operators
$a = 10;
$a &= 5;  // Bitwise AND assignment
echo $a . "\n";

$b = 8;
$b |= 2;  // Bitwise OR assignment  
echo $b . "\n";

$c = 4;
$c ^= 3;  // Bitwise XOR assignment
echo $c . "\n";

// Test complex assignment chain
$x = 16;
$x <<= 2;  // Left shift assignment
echo $x . "\n";

$y = 64;
$y >>= 3;  // Right shift assignment
echo $y . "\n";
?>
--EXPECT--
Scientific notation tests:
10000000000
0.0025
314
2500
0
10
7
64
8
--CLEAN--
<?php
unset($result, $a, $b, $c, $x, $y);
