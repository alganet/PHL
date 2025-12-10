--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_xdigit basic functionality
--FILE--
<?php
// Test basic ctype_xdigit functionality
$result1 = ctype_xdigit("123ABC");
echo $result1 ? "HEX_DIGITS_OK\n" : "HEX_DIGITS_FAIL\n";

// Test with lowercase hex
$result2 = ctype_xdigit("abcdef");
echo $result2 ? "LOWER_HEX_OK\n" : "LOWER_HEX_FAIL\n";

// Test with mixed case
$result3 = ctype_xdigit("123aBc");
echo $result3 ? "MIXED_CASE_OK\n" : "MIXED_CASE_FAIL\n";

// Test with numbers only
$result4 = ctype_xdigit("0123456789");
echo $result4 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with invalid characters
$result5 = ctype_xdigit("123GHI");
echo !$result5 ? "INVALID_OK\n" : "INVALID_FAIL\n";

// Test with letters beyond F
$result6 = ctype_xdigit("XYZ");
echo !$result6 ? "NON_HEX_OK\n" : "NON_HEX_FAIL\n";

// Test with spaces
$result7 = ctype_xdigit("123 ABC");
echo !$result7 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with empty string
$result8 = ctype_xdigit("");
echo !$result8 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result9 = ctype_xdigit("1A2B");
echo is_bool($result9) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
HEX_DIGITS_OK
LOWER_HEX_OK
MIXED_CASE_OK
NUMBERS_OK
INVALID_OK
NON_HEX_OK
SPACES_OK
EMPTY_OK
TYPE_OK
