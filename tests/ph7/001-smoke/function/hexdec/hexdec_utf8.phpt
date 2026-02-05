--CREDITS--
SPDX-FileCopyrightText: 2025 Cline <assistant@cline.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec handles UTF-8 characters in input string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test hexdec with UTF-8 characters before hex digits
// This should trigger the UTF-8 handling code in builtin.c
$result = hexdec("\xC2\xA9FF"); // UTF-8 copyright symbol followed by FF
echo "Result: " . $result . "\n";
echo "Expected: 255\n";
?>
--EXPECT--
Result: 255
Expected: 255
--CLEAN--
<?php
unset($result);
