--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex expression precedence and operator parsing edge cases
--FILE--
<?php
// Test complex expression parsing that exercises parser precedence logic
// This covers various operator precedence and complex expression handling

// Complex arithmetic with mixed operators
$result1 = 2 + 3 * 4 - 5 / 2;
echo "Arithmetic precedence: $result1\n"; // Should be 2 + 12 - 2.5 = 11.5

// Bitwise operations with complex precedence
$result2 = 8 | 4 & 2 ^ 1;
echo "Bitwise precedence: $result2\n"; // Should be 8 | (4 & 2) ^ 1 = 8 | 0 ^ 1 = 9

// Logical operators with mixed types
$result3 = (true && false) || (1 < 2) && (3 >= 2);
echo "Logical precedence: " . ($result3 ? 'true' : 'false') . "\n";

// Complex assignment with references
$a = 5;
$b = &$a;
$c = $b += 3;
echo "Reference assignment: a=$a, b=$b, c=$c\n";

// Ternary operator with complex conditions
$value = (5 > 3) ? ((2 * 5) < 10 ? 'small' : 'large') : 'false';
echo "Nested ternary: $value\n";

// Unary operators in complex expressions
$result4 = -5 + +3 * -2;
echo "Unary operators: $result4\n"; // Should be -5 + 3 * -2 = -5 + -6 = -11

// Complex function-like expression parsing
$result5 = (2 + 3) * (4 - 1) / (6 % 4);
echo "Parenthesized expressions: $result5\n"; // Should be 5 * 3 / 2 = 15 / 2 = 7.5
?>
--EXPECT--
Arithmetic precedence: 11.5
Bitwise precedence: 9
Logical precedence: true
Reference assignment: a=8, b=8, c=8
Nested ternary: large
Unary operators: -11
Parenthesized expressions: 7.5
--CLEAN--
<?php
unset($result1, $result2, $result3, $a, $b, $c, $value, $result4, $result5);
