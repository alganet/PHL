--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_alpha basic functionality
--FILE--
<?php
// Test basic ctype_alpha functionality
$result1 = ctype_alpha("Hello");
echo $result1 ? "LETTERS_OK\n" : "LETTERS_FAIL\n";

// Test with uppercase letters
$result2 = ctype_alpha("WORLD");
echo $result2 ? "UPPERCASE_OK\n" : "UPPERCASE_FAIL\n";

// Test with mixed case
$result3 = ctype_alpha("HelloWorld");
echo $result3 ? "MIXED_OK\n" : "MIXED_FAIL\n";

// Test with numbers
$result4 = ctype_alpha("abc123");
echo !$result4 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with special characters
$result5 = ctype_alpha("abc!");
echo !$result5 ? "SPECIAL_OK\n" : "SPECIAL_FAIL\n";

// Test with spaces
$result6 = ctype_alpha("hello world");
echo !$result6 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with empty string
$result7 = ctype_alpha("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_alpha("test");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
LETTERS_OK
UPPERCASE_OK
MIXED_OK
NUMBERS_OK
SPECIAL_OK
SPACES_OK
EMPTY_OK
TYPE_OK
