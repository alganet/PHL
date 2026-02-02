--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with special characters in separator
--FILE--
<?php
// Test implode with tab separator
$result1 = implode("\t", array("a", "b"));
echo $result1 === "a\tb" ? "TAB_SEP_OK\n" : "TAB_SEP_FAIL: $result1\n";

// Test implode with newline separator
$result2 = implode("\n", array("line1", "line2"));
echo $result2 === "line1\nline2" ? "NEWLINE_SEP_OK\n" : "NEWLINE_SEP_FAIL: $result2\n";

// Test implode with space separator
$result3 = implode(" ", array("hello", "world"));
echo $result3 === "hello world" ? "SPACE_SEP_OK\n" : "SPACE_SEP_FAIL: $result3\n";

// Test implode with special symbols
$result4 = implode("->", array("start", "end"));
echo $result4 === "start->end" ? "SYMBOLS_SEP_OK\n" : "SYMBOLS_SEP_FAIL: $result4\n";
?>
--EXPECT--
TAB_SEP_OK
NEWLINE_SEP_OK
SPACE_SEP_OK
SYMBOLS_SEP_OK