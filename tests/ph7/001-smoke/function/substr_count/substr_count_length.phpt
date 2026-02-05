--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_count with offset and length parameters
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test substr_count with 4 arguments (haystack, needle, offset, length)
$haystack = "abababababa";
$needle = "aba";

// Test with offset and length
$result1 = substr_count($haystack, $needle, 0, 6); // "ababab" -> 1 (at 0-2)
echo "substr_count with offset 0, length 6: $result1\n";

$result2 = substr_count($haystack, $needle, 2, 8); // "abababab" -> 2 (at 2-4, 4-6)
echo "substr_count with offset 2, length 8: $result2\n";

// Test with invalid length (too large)
$result3 = substr_count($haystack, $needle, 0, 100);
echo "substr_count with large length: $result3\n";
?>
--EXPECT--
substr_count with offset 0, length 6: 1
substr_count with offset 2, length 8: 2
substr_count with large length: 0
--CLEAN--
<?php
unset($haystack, $needle, $result1, $result2, $result3);
