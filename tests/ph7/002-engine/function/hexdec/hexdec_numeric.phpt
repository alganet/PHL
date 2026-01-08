--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec handles numeric (non-string) arguments
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
// Test hexdec with numeric argument - this covers the else branch in builtin.c
// Line 968-969: iVal = ph7_value_to_int64(apArg[0]);
echo "Test 1: hexdec with integer 255 (0xFF)" . "\n";
$result1 = hexdec(255);
echo "Result: " . $result1 . "\n";
echo "Expected: 255\n";

echo "Test 2: hexdec with integer 16 (0x10)" . "\n";
$result2 = hexdec(16);
echo "Result: " . $result2 . "\n";
echo "Expected: 16\n";

echo "Test 3: hexdec with large integer" . "\n";
$result3 = hexdec(123456789);
echo "Result: " . $result3 . "\n";
echo "Expected: 123456789\n";
?>
--EXPECT--
Test 1: hexdec with integer 255 (0xFF)
Result: 255
Expected: 255
Test 2: hexdec with integer 16 (0x10)
Result: 16
Expected: 16
Test 3: hexdec with large integer
Result: 123456789
Expected: 123456789