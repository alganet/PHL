--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: wordwrap basic functionality
--FILE--
<?php
// Test basic wordwrap functionality
$text = "The quick brown fox jumps over the lazy dog";
$result = wordwrap($text, 10);
echo strpos($result, "\n") !== false ? "WRAP_OK\n" : "WRAP_FAIL\n";

// Test with custom break
$result2 = wordwrap($text, 10, "<br>\n");
echo strpos($result2, "<br>\n") !== false ? "CUSTOM_BREAK_OK\n" : "CUSTOM_BREAK_FAIL\n";

// Test with longer width
$result3 = wordwrap($text, 50);
echo strpos($result3, "\n") === false ? "NO_WRAP_OK\n" : "NO_WRAP_FAIL\n";

// Test with very short width (breaks words when necessary)
$result4 = wordwrap("hello world", 5);
echo strpos($result4, "\n") !== false ? "SHORT_WRAP_OK\n" : "SHORT_WRAP_FAIL\n";

// Test empty string
$result5 = wordwrap("", 10);
echo strlen($result5) === 0 ? "EMPTY_OK\n" : "EMPTY_FAIL: '$result5'\n";

?>
--EXPECT--
WRAP_OK
CUSTOM_BREAK_OK
NO_WRAP_OK
SHORT_WRAP_OK
EMPTY_OK
--CLEAN--
<?php
unset($text, $result, $result2, $result3, $result4, $result5);
