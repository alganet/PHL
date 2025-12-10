--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strripos basic functionality
--FILE--
<?php
// Test basic strripos functionality
$result1 = strripos("Hello World World", "world");
echo $result1 === 12 ? "LAST_OCCURRENCE_OK\n" : "LAST_OCCURRENCE_FAIL: $result1\n";

// Test with case insensitivity
$result2 = strripos("Hello WORLD world", "WORLD");
echo $result2 === 12 ? "CASE_INSENSITIVE_OK\n" : "CASE_INSENSITIVE_FAIL: $result2\n";

// Test with substring not found
$result3 = strripos("Hello World", "xyz");
echo $result3 === false ? "NOT_FOUND_OK\n" : "NOT_FOUND_FAIL: '$result3'\n";

// Test that function returns integer for found substrings
$result4 = strripos("Hello World", "world");
echo is_int($result4) ? "FOUND_TYPE_OK\n" : "FOUND_TYPE_FAIL\n";

// Test case insensitivity works
$result5 = strripos("TEST", "t");
echo is_int($result5) ? "CASE_TYPE_OK\n" : "CASE_TYPE_FAIL\n";
?>
--EXPECT--
LAST_OCCURRENCE_OK
CASE_INSENSITIVE_OK
NOT_FOUND_OK
FOUND_TYPE_OK
CASE_TYPE_OK
