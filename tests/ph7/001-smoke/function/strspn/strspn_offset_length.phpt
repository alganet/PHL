--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strspn with offset and length parameters
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test strspn with offset and length parameters
$haystack = "1234567890abcdef";
$mask = "0123456789";

// Test basic offset and length
$result1 = strspn($haystack, $mask, 0, 5); // "12345" -> 5
echo "strspn with offset 0, length 5: $result1\n";

$result2 = strspn($haystack, $mask, 5, 5); // "67890" -> 5
echo "strspn with offset 5, length 5: $result2\n";

// Test with length that goes beyond string
$result3 = strspn($haystack, $mask, 0, 50);
echo "strspn with large length: $result3\n";

// Test with offset near end
$result4 = strspn($haystack, $mask, 10, 10); // "abcdef" -> 0
echo "strspn with offset 10, length 10: $result4\n";
?>
--EXPECT--
strspn with offset 0, length 5: 5
strspn with offset 5, length 5: 5
strspn with large length: 10
strspn with offset 10, length 10: 0
--CLEAN--
<?php
unset($haystack, $mask, $result1, $result2, $result3, $result4);
