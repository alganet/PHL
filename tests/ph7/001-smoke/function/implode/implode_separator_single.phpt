--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with single element array
--FILE--
<?php
// Test implode with single element array and separator
$result1 = implode(",", array("single"));
echo $result1 === "single" ? "SINGLE_WITH_SEP_OK\n" : "SINGLE_WITH_SEP_FAIL: $result1\n";

// Test implode with single element array and empty separator
$result2 = implode("", array("test"));
echo $result2 === "test" ? "SINGLE_EMPTY_SEP_OK\n" : "SINGLE_EMPTY_SEP_FAIL: $result2\n";

// Test implode with single numeric element
$result3 = implode("-", array(42));
echo $result3 === "42" ? "SINGLE_NUMERIC_OK\n" : "SINGLE_NUMERIC_FAIL: $result3\n";
?>
--EXPECT--
SINGLE_WITH_SEP_OK
SINGLE_EMPTY_SEP_OK
SINGLE_NUMERIC_OK
--CLEAN--
<?php
unset($result1, $result2, $result3);
