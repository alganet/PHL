--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ucwords basic functionality
--FILE--
<?php
// Test basic ucwords functionality
$result1 = ucwords("hello world");
echo $result1 === "Hello World" ? "BASIC_OK\n" : "BASIC_FAIL: '$result1'\n";

// Test with multiple words and punctuation
$result2 = ucwords("the quick brown fox jumps over the lazy dog.");
echo $result2 === "The Quick Brown Fox Jumps Over The Lazy Dog." ? "MULTI_OK\n" : "MULTI_FAIL: '$result2'\n";

// Test with already capitalized words
$result3 = ucwords("HELLO WORLD");
echo $result3 === "HELLO WORLD" ? "CAPITAL_OK\n" : "CAPITAL_FAIL: '$result3'\n";

// Test empty string
$result4 = ucwords("");
echo $result4 === "" ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result4'\n";

// Test single word
$result5 = ucwords("test");
echo $result5 === "Test" ? "SINGLE_OK\n" : "SINGLE_FAIL: '$result5'\n";

// Test with numbers and special characters
$result6 = ucwords("hello123 world!");
echo $result6 === "Hello123 World!" ? "SPECIAL_OK\n" : "SPECIAL_FAIL: '$result6'\n";
?>
--EXPECT--
BASIC_OK
MULTI_OK
CAPITAL_OK
EMPTY_OK
SINGLE_OK
SPECIAL_OK
