--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ireplace basic functionality
--FILE--
<?php
// Test basic str_ireplace functionality
$result1 = str_ireplace("world", "PHP", "Hello World");
echo $result1 === "Hello PHP" ? "BASIC_OK\n" : "BASIC_FAIL: '$result1'\n";

// Test case insensitive replacement
$result2 = str_ireplace("WORLD", "PHP", "Hello world");
echo $result2 === "Hello PHP" ? "CASE_OK\n" : "CASE_FAIL: '$result2'\n";

// Test multiple replacements
$result3 = str_ireplace("o", "0", "Hello World");
echo $result3 === "Hell0 W0rld" ? "MULTI_OK\n" : "MULTI_FAIL: '$result3'\n";

// Test with arrays (if supported)
$result4 = str_ireplace(array("hello", "world"), array("hi", "php"), "Hello World");
echo $result4 === "hi php" ? "ARRAY_OK\n" : "ARRAY_FAIL: '$result4'\n";

// Test no replacements
$result5 = str_ireplace("notfound", "replacement", "Hello World");
echo $result5 === "Hello World" ? "NOCHANGE_OK\n" : "NOCHANGE_FAIL: '$result5'\n";

// Test empty string
$result6 = str_ireplace("", "X", "abc");
echo $result6 === "abc" ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result6'\n";
?>
--EXPECT--
BASIC_OK
CASE_OK
MULTI_OK
ARRAY_OK
NOCHANGE_OK
EMPTY_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6);
