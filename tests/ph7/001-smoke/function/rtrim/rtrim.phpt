--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: rtrim basic functionality
--FILE--
<?php
// Test basic rtrim functionality
$result1 = rtrim("hello world  ");
echo $result1 === "hello world" ? "BASIC_OK\n" : "BASIC_FAIL: '$result1'\n";

// Test with tabs and newlines at end
$result2 = rtrim("hello world\t\n");
echo $result2 === "hello world" ? "WHITESPACE_OK\n" : "WHITESPACE_FAIL: '$result2'\n";

// Test with no whitespace at end
$result3 = rtrim("hello");
echo $result3 === "hello" ? "NO_TRIM_OK\n" : "NO_TRIM_FAIL: '$result3'\n";

// Test with whitespace at beginning (should not be removed)
$result4 = rtrim("  hello world");
echo $result4 === "  hello world" ? "LEFT_PRESERVE_OK\n" : "LEFT_PRESERVE_FAIL: '$result4'\n";

// Test with custom characters
$result5 = rtrim("helloxxx", "x");
echo $result5 === "hello" ? "CUSTOM_OK\n" : "CUSTOM_FAIL: '$result5'\n";

// Test empty string
$result6 = rtrim("");
echo strlen($result6) === 0 ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result6'\n";

// Test chop alias
$result7 = chop("test  ");
echo $result7 === "test" ? "CHOP_ALIAS_OK\n" : "CHOP_ALIAS_FAIL: '$result7'\n";
?>
--EXPECT--
BASIC_OK
WHITESPACE_OK
NO_TRIM_OK
LEFT_PRESERVE_OK
CUSTOM_OK
EMPTY_OK
CHOP_ALIAS_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6, $result7);
