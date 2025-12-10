--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strcoll basic functionality
--FILE--
<?php
// Test basic strcoll functionality
$result1 = strcoll("abc", "abc");
echo $result1 === 0 ? "EQUAL_OK\n" : "EQUAL_FAIL: $result1\n";

// Test less than comparison
$result2 = strcoll("abc", "def");
echo $result2 < 0 ? "LESS_OK\n" : "LESS_FAIL: $result2\n";

// Test greater than comparison
$result3 = strcoll("def", "abc");
echo $result3 > 0 ? "GREATER_OK\n" : "GREATER_FAIL: $result3\n";

// Test with different cases
$result4 = strcoll("abc", "ABC");
echo is_int($result4) ? "CASE_OK\n" : "CASE_FAIL: $result4\n";

// Test return type is integer
$result5 = strcoll("hello", "world");
echo is_int($result5) ? "TYPE_OK\n" : "TYPE_FAIL\n";

// Test empty strings
$result6 = strcoll("", "");
echo $result6 === 0 ? "EMPTY_OK\n" : "EMPTY_FAIL: $result6\n";
?>
--EXPECT--
EQUAL_OK
LESS_OK
GREATER_OK
CASE_OK
TYPE_OK
EMPTY_OK
