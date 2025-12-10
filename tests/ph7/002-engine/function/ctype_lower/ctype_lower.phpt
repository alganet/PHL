--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_lower basic functionality
--FILE--
<?php
// Test basic ctype_lower functionality
$result1 = ctype_lower("hello");
echo $result1 ? "LOWERCASE_OK\n" : "LOWERCASE_FAIL\n";

// Test with mixed case
$result2 = ctype_lower("Hello");
echo !$result2 ? "MIXED_OK\n" : "MIXED_FAIL\n";

// Test with uppercase
$result3 = ctype_lower("WORLD");
echo !$result3 ? "UPPERCASE_OK\n" : "UPPERCASE_FAIL\n";

// Test with numbers
$result4 = ctype_lower("abc123");
echo !$result4 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with special characters
$result5 = ctype_lower("abc!");
echo !$result5 ? "SPECIAL_OK\n" : "SPECIAL_FAIL\n";

// Test with spaces
$result6 = ctype_lower("hello world");
echo !$result6 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with empty string
$result7 = ctype_lower("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_lower("test");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
LOWERCASE_OK
MIXED_OK
UPPERCASE_OK
NUMBERS_OK
SPECIAL_OK
SPACES_OK
EMPTY_OK
TYPE_OK
