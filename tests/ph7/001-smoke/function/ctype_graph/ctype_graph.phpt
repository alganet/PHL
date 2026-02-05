--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_graph basic functionality
--FILE--
<?php
// Test basic ctype_graph functionality
$result1 = ctype_graph("abc123!@#");
echo $result1 ? "PRINTABLE_OK\n" : "PRINTABLE_FAIL\n";

// Test with letters and numbers
$result2 = ctype_graph("Hello123");
echo $result2 ? "ALPHANUM_OK\n" : "ALPHANUM_FAIL\n";

// Test with punctuation
$result3 = ctype_graph("!@#$%^&*()");
echo $result3 ? "PUNCTUATION_OK\n" : "PUNCTUATION_FAIL\n";

// Test with space (should fail)
$result4 = ctype_graph("hello world");
echo !$result4 ? "SPACE_FAIL_OK\n" : "SPACE_FAIL_FAIL\n";

// Test with tab (should fail)
$result5 = ctype_graph("hello\tworld");
echo !$result5 ? "TAB_FAIL_OK\n" : "TAB_FAIL_FAIL\n";

// Test with newline (should fail)
$result6 = ctype_graph("hello\nworld");
echo !$result6 ? "NEWLINE_FAIL_OK\n" : "NEWLINE_FAIL_FAIL\n";

// Test with empty string
$result7 = ctype_graph("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_graph("test123!");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
PRINTABLE_OK
ALPHANUM_OK
PUNCTUATION_OK
SPACE_FAIL_OK
TAB_FAIL_OK
NEWLINE_FAIL_OK
EMPTY_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6, $result7, $result8);
