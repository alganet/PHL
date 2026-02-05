--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with empty separator
--FILE--
<?php
// Test implode with single element array and empty separator
$result1 = implode("", array("single"));
echo $result1 === "single" ? "SINGLE_ELEMENT_OK\n" : "SINGLE_ELEMENT_FAIL: $result1\n";

// Test implode with two elements and empty separator
$result2 = implode("", array("a", "b"));
echo $result2 === "ab" ? "TWO_ELEMENTS_OK\n" : "TWO_ELEMENTS_FAIL: $result2\n";

// Test implode with multiple elements and empty separator
$result3 = implode("", array("x", "y", "z"));
echo $result3 === "xyz" ? "MULTIPLE_ELEMENTS_OK\n" : "MULTIPLE_ELEMENTS_FAIL: $result3\n";
?>
--EXPECT--
SINGLE_ELEMENT_OK
TWO_ELEMENTS_OK
MULTIPLE_ELEMENTS_OK
--CLEAN--
<?php
unset($result1, $result2, $result3);
