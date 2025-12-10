--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: ctype_upper basic functionality
--FILE--
<?php
// Test basic ctype_upper functionality
$result1 = ctype_upper("HELLO");
echo $result1 ? "UPPERCASE_OK\n" : "UPPERCASE_FAIL\n";

// Test with mixed case
$result2 = ctype_upper("Hello");
echo !$result2 ? "MIXED_OK\n" : "MIXED_FAIL\n";

// Test with lowercase
$result3 = ctype_upper("world");
echo !$result3 ? "LOWERCASE_OK\n" : "LOWERCASE_FAIL\n";

// Test with numbers
$result4 = ctype_upper("ABC123");
echo !$result4 ? "NUMBERS_OK\n" : "NUMBERS_FAIL\n";

// Test with special characters
$result5 = ctype_upper("ABC!");
echo !$result5 ? "SPECIAL_OK\n" : "SPECIAL_FAIL\n";

// Test with spaces
$result6 = ctype_upper("HELLO WORLD");
echo !$result6 ? "SPACES_OK\n" : "SPACES_FAIL\n";

// Test with empty string
$result7 = ctype_upper("");
echo !$result7 ? "EMPTY_OK\n" : "EMPTY_FAIL\n";

// Test return type
$result8 = ctype_upper("TEST");
echo is_bool($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
UPPERCASE_OK
MIXED_OK
LOWERCASE_OK
NUMBERS_OK
SPECIAL_OK
SPACES_OK
EMPTY_OK
TYPE_OK
