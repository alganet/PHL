--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: implode with separator edge cases
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test implode with single element array and separator
$result1 = implode(",", array("single"));
echo $result1 === "single" ? "SINGLE_ELEMENT_OK\n" : "SINGLE_ELEMENT_FAIL: $result1\n";

// Test implode with empty separator on single element
$result2 = implode("", array("test"));
echo $result2 === "test" ? "EMPTY_SEP_SINGLE_OK\n" : "EMPTY_SEP_SINGLE_FAIL: $result2\n";

// Test implode with two elements
$result3 = implode(",", array("a", "b"));
echo $result3 === "a,b" ? "TWO_ELEMENTS_OK\n" : "TWO_ELEMENTS_FAIL: $result3\n";

// Test implode with numeric elements
$result4 = implode("-", array(1, 2, 3));
echo $result4 === "1-2-3" ? "NUMERIC_ELEMENTS_OK\n" : "NUMERIC_ELEMENTS_FAIL: $result4\n";

// Test implode with mixed types
$result5 = implode("|", array("a", 1, "b", 2));
echo $result5 === "a|1|b|2" ? "MIXED_TYPES_OK\n" : "MIXED_TYPES_FAIL: $result5\n";

// Test implode with long separator
$result6 = implode(" SEP ", array("a", "b", "c"));
echo $result6 === "a SEP b SEP c" ? "LONG_SEP_OK\n" : "LONG_SEP_FAIL: $result6\n";

// Test implode with special characters in separator
$result7 = implode("\t", array("a", "b"));
echo $result7 === "a\tb" ? "TAB_SEP_OK\n" : "TAB_SEP_FAIL: $result7\n";

// Test implode with newline separator
$result8 = implode("\n", array("line1", "line2"));
echo $result8 === "line1\nline2" ? "NEWLINE_SEP_OK\n" : "NEWLINE_SEP_FAIL: $result8\n";

// Test implode with boolean values - PHL skips false (treats as empty)
$result9 = implode(",", array(true, false, true));
echo $result9 === "TRUE,TRUE" ? "BOOL_VALUES_OK\n" : "BOOL_VALUES_FAIL: $result9\n";

// Test implode with null values
$result10 = implode("-", array("a", null, "b"));
echo $result10 === "a-b" ? "NULL_VALUES_OK\n" : "NULL_VALUES_FAIL: $result10\n";
?>
--EXPECT--
SINGLE_ELEMENT_OK
EMPTY_SEP_SINGLE_OK
TWO_ELEMENTS_OK
NUMERIC_ELEMENTS_OK
MIXED_TYPES_OK
LONG_SEP_OK
TAB_SEP_OK
NEWLINE_SEP_OK
BOOL_VALUES_OK
NULL_VALUES_OK