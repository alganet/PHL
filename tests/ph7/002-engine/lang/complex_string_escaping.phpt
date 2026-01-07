--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex string escaping edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test complex string escaping that exercises lexer edge cases
// This covers various escape sequence handling in strings

// Single quoted strings with escaped single quotes
$single1 = 'Don\'t worry';
$single2 = 'It\'s a test';
echo "Single quoted: $single1 - $single2\n";

// Double quoted strings with various escapes
$double1 = "Line 1\nLine 2\tTabbed\rCarriage Return";
$double2 = "Quote: \"Hello\" and backslash: \\";
echo "Double quoted: $double1\n$double2\n";

// Complex escape sequences
$complex1 = "Hex: \x48\x65\x6c\x6c\x6f";
$complex2 = "Octal: \110\145\154\154\157";
echo "Complex: $complex1 - $complex2\n";

// Edge case: escaped backslash at end of string
$edge1 = "Ends with backslash\\";
$edge2 = 'Ends with backslash\\';
echo "Edge cases: '$edge1' and '$edge2'\n";

// Multiple consecutive escapes
$multi = "Multiple\\\\\\escapes";
echo "Multiple: $multi\n";
?>
--EXPECT--
Single quoted: Don't worry - It's a test
Double quoted: Line 1
Line 2	TabbedCarriage Return
Quote: "Hello" and backslash: \
Complex: Hex: Hello - Octal: 110145154154157
Edge cases: 'Ends with backslash\' and 'Ends with backslash\'
Multiple: Multiple\\\escapes