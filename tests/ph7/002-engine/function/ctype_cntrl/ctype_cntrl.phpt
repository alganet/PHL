--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_cntrl basic functionality
--FILE--
<?php
// Test basic ctype_cntrl functionality
$result1 = ctype_cntrl("\n\t\r");
echo $result1 ? "CONTROL_CHARS_OK\n" : "CONTROL_CHARS_FAIL\n";

// Test with null character
$result2 = ctype_cntrl("\0");
echo $result2 ? "NULL_CHAR_OK\n" : "NULL_CHAR_FAIL\n";

// Test with backspace
$result3 = ctype_cntrl("\x08");
echo $result3 ? "BACKSPACE_OK\n" : "BACKSPACE_FAIL\n";

// Test with regular characters
$result4 = ctype_cntrl("hello");
echo !$result4 ? "LETTERS_OK\n" : "LETTERS_FAIL\n";

// Test with numbers
$result5 = ctype_cntrl("123");
echo !$result5 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with mixed content
$result6 = ctype_cntrl("\thello\n");
echo !$result6 ? "MIXED_OK\n" : "MIXED_FAIL\n";

// Test with empty string
$result7 = ctype_cntrl("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_cntrl("\n");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
CONTROL_CHARS_OK
NULL_CHAR_OK
BACKSPACE_OK
LETTERS_OK
NUMBERS_OK
MIXED_OK
EMPTY_OK
TYPE_OK
