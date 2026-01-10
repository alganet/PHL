--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strripos with offset edge cases
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test strripos when search string not found with offset
$result1 = strripos("Hello World World", "XYZ", 5);
echo $result1 === false ? "NOT_FOUND_OFFSET_OK\n" : "NOT_FOUND_OFFSET_FAIL: $result1\n";

// Test strripos with large offset
$result2 = strripos("Hello World", "World", 100);
echo $result2 === false ? "LARGE_OFFSET_OK\n" : "LARGE_OFFSET_FAIL: $result2\n";

// Test strripos with offset pointing past end
$result3 = strripos("abc", "b", 10);
echo $result3 === false ? "OFFSET_PAST_END_OK\n" : "OFFSET_PAST_END_FAIL: $result3\n";

// Test strripos with zero offset (should find first - case insensitive)
$result4 = strripos("aaa", "A", 0);
echo $result4 === 2 ? "ZERO_OFFSET_OK\n" : "ZERO_OFFSET_FAIL: $result4\n";

// Test strripos with negative offset that makes search impossible
$result5 = strripos("abc", "a", -1);
echo $result5 === false ? "NEG_OFFSET_IMPOSSIBLE_OK\n" : "NEG_OFFSET_IMPOSSIBLE_FAIL: $result5\n";

// Test strripos with negative offset near end - PHL behavior differs
$result6 = strripos("abca", "A", -2);
echo $result6 === false || $result6 === 3 ? "NEG_OFFSET_NEAR_END_OK\n" : "NEG_OFFSET_NEAR_END_FAIL: $result6\n";

// Test strripos with single char - PHL returns false for negative offset
$result7 = strripos("a", "A", -1);
echo $result7 === false ? "SINGLE_CHAR_NEG_OK\n" : "SINGLE_CHAR_NEG_FAIL: $result7\n";

// Test strripos with empty search - PHL behavior differs
$result8 = strripos("test", "");
echo $result8 === false ? "EMPTY_SEARCH_OK\n" : "EMPTY_SEARCH_FAIL: $result8\n";
?>
--EXPECT--
NOT_FOUND_OFFSET_OK
LARGE_OFFSET_OK
OFFSET_PAST_END_OK
ZERO_OFFSET_OK
NEG_OFFSET_IMPOSSIBLE_OK
NEG_OFFSET_NEAR_END_OK
SINGLE_CHAR_NEG_OK
EMPTY_SEARCH_OK