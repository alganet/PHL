--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Lexer EOF handling and operator extraction edge cases
--FILE--
<?php
// Test end of input scenarios
// Test alpha stream operators with ambiguity
$a = true and false;
$b = true or false;
$c = true xor false;

// Test operator precedence with ambiguous operators
$result1 = 5 + 3 * 2; // Should be 11, not 16
$result2 = (5 + 3) * 2; // Should be 16

echo "result1: $result1\n";
echo "result2: $result2\n";

// Test unary vs binary operators
$pos = +5;
$neg = -5;
echo "positive: $pos, negative: $neg\n";

// Test string concatenation vs addition
$str = "hello" . " world";
$num = 5 + 3;
echo "string: $str, number: $num\n";
?>
--EXPECT--
result1: 11
result2: 16
positive: 5, negative: -5
string: hello world, number: 8