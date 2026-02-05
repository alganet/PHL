--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strrpos basic functionality
--FILE--
<?php
// Test basic strrpos functionality
$result1 = strrpos("Hello World World", "World");
echo $result1 === 12 ? "LAST_OCCURRENCE_OK\n" : "LAST_OCCURRENCE_FAIL: $result1\n";

// Test with substring not found
$result2 = strrpos("Hello World", "xyz");
echo $result2 === false ? "NOT_FOUND_OK\n" : "NOT_FOUND_FAIL: $result2\n";

// Test that function returns integer for found substrings
$result3 = strrpos("Hello World", "World");
echo is_int($result3) ? "FOUND_TYPE_OK\n" : "FOUND_TYPE_FAIL\n";

// Test with offset
$result4 = strrpos("Hello World World Test", "World", 10);
echo is_int($result4) ? "OFFSET_OK\n" : "OFFSET_FAIL: $result4\n";

// Test with negative offset
$result5 = strrpos("Hello World World Test", "World", -10);
echo is_int($result5) ? "NEG_OFFSET_OK\n" : "NEG_OFFSET_FAIL: $result5\n";

// Test return type
$result6 = strrpos("test", "t");
echo is_int($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
LAST_OCCURRENCE_OK
NOT_FOUND_OK
FOUND_TYPE_OK
OFFSET_OK
NEG_OFFSET_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6);
