--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_print basic functionality
--FILE--
<?php
// Test basic ctype_print functionality
$result1 = ctype_print("Hello World!");
echo $result1 ? "PRINTABLE_OK\n" : "PRINTABLE_FAIL\n";

// Test with numbers and punctuation
$result2 = ctype_print("abc123!@#");
echo $result2 ? "ALPHANUM_PUNCT_OK\n" : "ALPHANUM_PUNCT_FAIL\n";

// Test with space
$result3 = ctype_print("hello world");
echo $result3 ? "SPACE_OK\n" : "SPACE_FAIL\n";

// Test with tab (should fail)
$result4 = ctype_print("hello\tworld");
echo !$result4 ? "TAB_FAIL_OK\n" : "TAB_FAIL_FAIL\n";

// Test with newline (should fail)
$result5 = ctype_print("hello\nworld");
echo !$result5 ? "NEWLINE_FAIL_OK\n" : "NEWLINE_FAIL_FAIL\n";

// Test with control character (should fail)
$result6 = ctype_print("hello\x00world");
echo !$result6 ? "CONTROL_FAIL_OK\n" : "CONTROL_FAIL_FAIL\n";

// Test with empty string
$result7 = ctype_print("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_print("test 123!");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
PRINTABLE_OK
ALPHANUM_PUNCT_OK
SPACE_OK
TAB_FAIL_OK
NEWLINE_FAIL_OK
CONTROL_FAIL_OK
EMPTY_OK
TYPE_OK
