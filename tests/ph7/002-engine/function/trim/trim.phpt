--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: trim basic functionality
--FILE--
<?php
// Test basic trim functionality
$result1 = trim("  hello world  ");
echo $result1 === "hello world" ? "BASIC_OK\n" : "BASIC_FAIL: '$result1'\n";

// Test with tabs and newlines
$result2 = trim("\t\n hello world \t\n");
echo $result2 === "hello world" ? "WHITESPACE_OK\n" : "WHITESPACE_FAIL: '$result2'\n";

// Test with no whitespace
$result3 = trim("hello");
echo $result3 === "hello" ? "NO_TRIM_OK\n" : "NO_TRIM_FAIL: '$result3'\n";

// Test with custom characters
$result4 = trim("xxxhelloxxx", "x");
echo $result4 === "hello" ? "CUSTOM_OK\n" : "CUSTOM_FAIL: '$result4'\n";

// Test empty string
$result5 = trim("");
echo strlen($result5) === 0 ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result5'\n";

// Test return type
$result6 = trim(" test ");
echo is_string($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
BASIC_OK
WHITESPACE_OK
NO_TRIM_OK
CUSTOM_OK
EMPTY_OK
TYPE_OK
