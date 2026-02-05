--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
strrchr with integer as second argument (needle)
--FILE--
<?php
// Test strrchr with integer as second argument (needle)
// When the second argument is an integer, it should be treated as a character code

// Test with ASCII code for 'o' (111)
$result1 = strrchr("hello world", 111);
echo ($result1 === "orld") ? "INT_111_OK\n" : "INT_111_FAIL: '$result1'\n";

// Test with ASCII code for 'l' (108)
$result2 = strrchr("hello world", 108);
echo ($result2 === "ld") ? "INT_108_OK\n" : "INT_108_FAIL: '$result2'\n";

// Test with ASCII code for 'd' (100)
$result3 = strrchr("hello world", 100);
echo ($result3 === "d") ? "INT_100_OK\n" : "INT_100_FAIL: '$result3'\n";

// Test with zero
$result4 = strrchr("hello world", 0);
echo ($result4 === false) ? "INT_0_OK\n" : "INT_0_FAIL: '$result4'\n";

// Test with character not in string using integer
$result5 = strrchr("hello world", 122); // 'z'
echo ($result5 === false) ? "INT_NOT_FOUND_OK\n" : "INT_NOT_FOUND_FAIL: '$result5'\n";
?>
--EXPECT--
INT_111_OK
INT_108_OK
INT_100_OK
INT_0_OK
INT_NOT_FOUND_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5);
