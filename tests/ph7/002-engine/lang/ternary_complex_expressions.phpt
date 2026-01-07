--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test complex expression parsing to cover ternary operator and precedence edge cases
--FILE--
<?php
// Test complex expressions to cover uncovered ternary operator and precedence handling

// Test ternary operator edge cases
$a = 5;
$b = 10;

// Basic ternary
$result = $a > $b ? $a : $b;
echo "Basic ternary: $result\n";

// Nested ternary
$result2 = $a > $b ? ($a > 15 ? "big" : "medium") : ($b > 5 ? "small" : "tiny");
echo "Nested ternary: $result2\n";

// Test complex expressions with mixed operators
$d = ($a + $b) * 2 > 20 ? $a * $b : $a + $b;
echo "Complex expression: $d\n";

// Test reference operator edge cases
$e = 42;
$f = &$e;  // Reference assignment
echo "Reference test: $f\n";

// Test assignment operator precedence
$g = $h = 15; // Chained assignment
echo "Chained assignment: $g, $h\n";

// Test bitwise and shift operators
$i = 5 << 2; // Left shift
$j = 20 >> 1; // Right shift
echo "Shift operators: $i, $j\n";

// Test bitwise operators
$k = 5 & 3; // AND
$l = 5 | 3; // OR
$m = 5 ^ 3; // XOR
echo "Bitwise operators: $k, $l, $m\n";

echo "Test completed\n";
?>
--EXPECT--
Basic ternary: 10
Nested ternary: small
Complex expression: 50
Reference test: 42
Chained assignment: 15, 15
Shift operators: 20, 10
Bitwise operators: 1, 7, 6
Test completed