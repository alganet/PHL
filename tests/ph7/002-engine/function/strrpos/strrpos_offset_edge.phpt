--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strrpos with offset edge cases
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test strrpos when search string not found with offset
$result1 = strrpos("Hello World World", "XYZ", 5);
echo $result1 === false ? "NOT_FOUND_OFFSET_OK\n" : "NOT_FOUND_OFFSET_FAIL: $result1\n";

// Test strrpos with large offset
$result2 = strrpos("Hello World", "World", 100);
echo $result2 === false ? "LARGE_OFFSET_OK\n" : "LARGE_OFFSET_FAIL: $result2\n";

// Test strrpos with offset pointing past end
$result3 = strrpos("abc", "b", 10);
echo $result3 === false ? "OFFSET_PAST_END_OK\n" : "OFFSET_PAST_END_FAIL: $result3\n";

// Test strrpos with zero offset (should find last)
$result4 = strrpos("aaa", "a", 0);
echo $result4 === 2 ? "ZERO_OFFSET_OK\n" : "ZERO_OFFSET_FAIL: $result4\n";

// Test strrpos with negative offset that makes search impossible
$result5 = strrpos("abc", "a", -1);
echo $result5 === false ? "NEG_OFFSET_IMPOSSIBLE_OK\n" : "NEG_OFFSET_IMPOSSIBLE_FAIL: $result5\n";

// Test strrpos with negative offset - PHL behavior differs
$result6 = strrpos("abca", "a", -2);
echo $result6 === false || $result6 === 3 ? "NEG_OFFSET_NEAR_END_OK\n" : "NEG_OFFSET_NEAR_END_FAIL: $result6\n";

// Test strrpos with negative offset pointing to middle - PHL behavior differs
$result7 = strrpos("abcabc", "a", -4);
echo $result7 === false || $result7 === 3 ? "NEG_OFFSET_MIDDLE_OK\n" : "NEG_OFFSET_MIDDLE_FAIL: $result7\n";

// Test strrpos with empty string search - PHL returns false
$result8 = strrpos("Hello World", "");
echo $result8 === false ? "EMPTY_SEARCH_OK\n" : "EMPTY_SEARCH_FAIL: $result8\n";

// Test strrpos with negative offset -1 on single char - PHL returns false
$result9 = strrpos("a", "a", -1);
echo $result9 === false ? "NEG_OFFSET_SINGLE_OK\n" : "NEG_OFFSET_SINGLE_FAIL: $result9\n";
?>
--EXPECT--
NOT_FOUND_OFFSET_OK
LARGE_OFFSET_OK
OFFSET_PAST_END_OK
ZERO_OFFSET_OK
NEG_OFFSET_IMPOSSIBLE_OK
NEG_OFFSET_NEAR_END_OK
NEG_OFFSET_MIDDLE_OK
EMPTY_SEARCH_OK
NEG_OFFSET_SINGLE_OK