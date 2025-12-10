--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_shuffle basic functionality
--FILE--
<?php
// Test basic str_shuffle functionality
$original = "abcdef";
$shuffled = str_shuffle($original);

// Test that shuffled string has same length
echo strlen($shuffled) === strlen($original) ? "LENGTH_OK\n" : "LENGTH_FAIL\n";

// Test that shuffled string is not necessarily the same as original
// (though it could be by chance)
echo is_string($shuffled) ? "TYPE_OK\n" : "TYPE_FAIL\n";

// Test with repeated characters - check length preservation
$repeated = "aabbcc";
$shuffled2 = str_shuffle($repeated);
echo strlen($shuffled2) === strlen($repeated) ? "REPEATED_OK\n" : "REPEATED_FAIL\n";

// Test empty string
$empty = str_shuffle("");
echo strlen($empty) === 0 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test single character
$single = str_shuffle("x");
echo $single === "x" ? "SINGLE_OK\n" : "SINGLE_FAIL\n";

// Test function exists and returns string
$result = str_shuffle("test");
echo is_string($result) && strlen($result) === 4 ? "FUNCTION_OK\n" : "FUNCTION_FAIL\n";
?>
--EXPECT--
LENGTH_OK
TYPE_OK
REPEATED_OK
EMPTY_OK
SINGLE_OK
FUNCTION_OK
