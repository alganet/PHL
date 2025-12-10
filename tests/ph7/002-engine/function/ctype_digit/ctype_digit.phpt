--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_digit basic functionality
--FILE--
<?php
// Test basic ctype_digit functionality
$result1 = ctype_digit("12345");
echo $result1 ? "DIGITS_OK\n" : "DIGITS_FAIL\n";

// Test with single digit
$result2 = ctype_digit("7");
echo $result2 ? "SINGLE_OK\n" : "SINGLE_FAIL\n";

// Test with leading zero
$result3 = ctype_digit("007");
echo $result3 ? "LEADING_ZERO_OK\n" : "LEADING_ZERO_FAIL\n";

// Test with letters
$result4 = ctype_digit("abc123");
echo !$result4 ? "LETTERS_OK\n" : "LETTERS_FAIL\n";

// Test with special characters
$result5 = ctype_digit("123!");
echo !$result5 ? "SPECIAL_OK\n" : "SPECIAL_FAIL\n";

// Test with spaces
$result6 = ctype_digit("123 456");
echo !$result6 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with decimal point
$result7 = ctype_digit("123.45");
echo !$result7 ? "DECIMAL_OK\n" : "DECIMAL_FAIL\n";

// Test with empty string
$result8 = ctype_digit("");
echo !$result8 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result9 = ctype_digit("42");
echo is_bool($result9) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
DIGITS_OK
SINGLE_OK
LEADING_ZERO_OK
LETTERS_OK
SPECIAL_OK
SPACES_OK
DECIMAL_OK
EMPTY_OK
TYPE_OK
