--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stripos basic functionality
--FILE--
<?php
// Test basic stripos functionality
$result1 = stripos("Hello World", "world");
echo $result1 === 6 ? "CASE_INSENSITIVE_OK\n" : "CASE_INSENSITIVE_FAIL: $result1\n";

// Test with substring at the beginning
$result2 = stripos("Hello World", "HELLO");
echo $result2 === 0 ? "BEGINNING_OK\n" : "BEGINNING_FAIL: $result2\n";

// Test with substring not found
$result3 = stripos("Hello World", "xyz");
echo $result3 === false ? "NOT_FOUND_OK\n" : "NOT_FOUND_FAIL: $result3\n";

// Test with offset
$result4 = stripos("Hello World World", "world", 7);
echo $result4 === 12 ? "OFFSET_OK\n" : "OFFSET_FAIL: $result4\n";

// Test with negative offset
$result5 = stripos("Hello World World", "world", -5);
echo is_int($result5) ? "NEG_OFFSET_OK\n" : "NEG_OFFSET_FAIL: $result5\n";

// Test return type
$result6 = stripos("test", "T");
echo is_int($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
CASE_INSENSITIVE_OK
BEGINNING_OK
NOT_FOUND_OK
OFFSET_OK
NEG_OFFSET_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6);
