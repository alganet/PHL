--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_space basic functionality
--FILE--
<?php
// Test basic ctype_space functionality
$result1 = ctype_space(" \t\n\r");
echo $result1 ? "WHITESPACE_OK\n" : "WHITESPACE_FAIL\n";

// Test with space only
$result2 = ctype_space("   ");
echo $result2 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with tab only
$result3 = ctype_space("\t\t");
echo $result3 ? "TABS_OK\n" : "TABS_FAIL\n";

// Test with newline
$result4 = ctype_space("\n\r");
echo $result4 ? "NEWLINES_OK\n" : "NEWLINES_FAIL\n";

// Test with letters
$result5 = ctype_space("hello");
echo !$result5 ? "LETTERS_OK\n" : "LETTERS_FAIL\n";

// Test with numbers
$result6 = ctype_space("123");
echo !$result6 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with mixed content
$result7 = ctype_space(" \t hello \n");
echo !$result7 ? "MIXED_OK\n" : "MIXED_FAIL\n";

// Test with empty string
$result8 = ctype_space("");
echo !$result8 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result9 = ctype_space(" \t ");
echo is_bool($result9) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
WHITESPACE_OK
SPACES_OK
TABS_OK
NEWLINES_OK
LETTERS_OK
NUMBERS_OK
MIXED_OK
EMPTY_OK
TYPE_OK
