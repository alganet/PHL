--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strrchr basic functionality
--FILE--
<?php
// Test basic strrchr functionality
$result1 = strrchr("hello world", "o");
echo $result1 === "orld" ? "LAST_O_OK\n" : "LAST_O_FAIL: '$result1'\n";

// Test with character at the beginning
$result2 = strrchr("hello world", "h");
echo $result2 === "hello world" ? "FIRST_H_OK\n" : "FIRST_H_FAIL: '$result2'\n";

// Test with character not found
$result3 = strrchr("hello world", "z");
echo $result3 === false ? "NOT_FOUND_OK\n" : "NOT_FOUND_FAIL: '$result3'\n";

// Test with multiple occurrences
$result4 = strrchr("banana", "a");
echo $result4 === "a" ? "MULTIPLE_A_OK\n" : "MULTIPLE_A_FAIL: '$result4'\n";

// Test with empty string
$result5 = strrchr("", "a");
echo $result5 === false ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result5'\n";

// Test return type for found character
$result6 = strrchr("test", "t");
echo is_string($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
LAST_O_OK
FIRST_H_OK
NOT_FOUND_OK
MULTIPLE_A_OK
EMPTY_OK
TYPE_OK
