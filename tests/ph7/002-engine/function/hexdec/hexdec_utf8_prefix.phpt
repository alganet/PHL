--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: hexdec with UTF-8 characters before hex digits
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test hexdec with UTF-8 characters before hex digits - covers UTF-8 skipping loop
// UTF-8 'ñ' (0xc3 0xb1) followed by 'FF'
$test_string = "\xc3\xb1FF";
$result = hexdec($test_string);
echo "hexdec('\xc3\xb1FF') = " . $result . "\n";
echo "Expected: 255 (0xFF)\n";
if ($result === 255) {
    echo "PASS\n";
} else {
    echo "FAIL\n";
}
?>
--EXPECT--
hexdec('ñFF') = 255
Expected: 255 (0xFF)
PASS