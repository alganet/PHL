--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with long separator
--FILE--
<?php
// Test implode with long separator
$result1 = implode(" SEP ", array("a", "b", "c"));
echo $result1 === "a SEP b SEP c" ? "LONG_SEP_OK\n" : "LONG_SEP_FAIL: $result1\n";

// Test implode with very long separator
$result2 = implode("->", array("start", "middle", "end"));
echo $result2 === "start->middle->end" ? "VERY_LONG_SEP_OK\n" : "VERY_LONG_SEP_FAIL: $result2\n";

// Test implode with multi-character separator
$result3 = implode(" | ", array("x", "y"));
echo $result3 === "x | y" ? "MULTI_CHAR_SEP_OK\n" : "MULTI_CHAR_SEP_FAIL: $result3\n";
?>
--EXPECT--
LONG_SEP_OK
VERY_LONG_SEP_OK
MULTI_CHAR_SEP_OK