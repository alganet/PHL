--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_alnum basic functionality
--FILE--
<?php
// Test basic ctype_alnum functionality
$result1 = ctype_alnum("abc123");
echo $result1 ? "ALNUM_OK\n" : "ALNUM_FAIL\n";

// Test with letters only
$result2 = ctype_alnum("Hello");
echo $result2 ? "LETTERS_OK\n" : "LETTERS_FAIL\n";

// Test with numbers only
$result3 = ctype_alnum("12345");
echo $result3 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with special characters
$result4 = ctype_alnum("abc!@#");
echo !$result4 ? "SPECIAL_CHARS_OK\n" : "SPECIAL_CHARS_FAIL\n";

// Test with spaces
$result5 = ctype_alnum("abc 123");
echo !$result5 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with empty string
$result6 = ctype_alnum("");
echo !$result6 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result7 = ctype_alnum("test");
echo is_bool($result7) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ALNUM_OK
LETTERS_OK
NUMBERS_OK
SPECIAL_CHARS_OK
SPACES_OK
EMPTY_OK
TYPE_OK
