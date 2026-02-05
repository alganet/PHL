--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_punct basic functionality
--FILE--
<?php
// Test basic ctype_punct functionality
$result1 = ctype_punct("!@#$%^&*()");
echo $result1 ? "PUNCTUATION_OK\n" : "PUNCTUATION_FAIL\n";

// Test with various punctuation marks
$result2 = ctype_punct(".,:;!?");
echo $result2 ? "VARIOUS_PUNCT_OK\n" : "VARIOUS_PUNCT_FAIL\n";

// Test with brackets and quotes
$result3 = ctype_punct("[]{}()\"'");
echo $result3 ? "BRACKETS_QUOTES_OK\n" : "BRACKETS_QUOTES_FAIL\n";

// Test with letters (should fail)
$result4 = ctype_punct("hello");
echo !$result4 ? "LETTERS_FAIL_OK\n" : "LETTERS_FAIL_FAIL\n";

// Test with numbers (should fail)
$result5 = ctype_punct("123");
echo !$result5 ? "NUMBERS_FAIL_OK\n" : "NUMBERS_FAIL_FAIL\n";

// Test with spaces (should fail)
$result6 = ctype_punct("! @ #");
echo !$result6 ? "SPACES_FAIL_OK\n" : "SPACES_FAIL_FAIL\n";

// Test with mixed content (should fail)
$result7 = ctype_punct("!hello?");
echo !$result7 ? "MIXED_FAIL_OK\n" : "MIXED_FAIL_FAIL\n";

// Test with empty string
$result8 = ctype_punct("");
echo !$result8 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result9 = ctype_punct("!?.,");
echo is_bool($result9) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
PUNCTUATION_OK
VARIOUS_PUNCT_OK
BRACKETS_QUOTES_OK
LETTERS_FAIL_OK
NUMBERS_FAIL_OK
SPACES_FAIL_OK
MIXED_FAIL_OK
EMPTY_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6, $result7, $result8, $result9);
