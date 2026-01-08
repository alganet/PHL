--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strftime with %D format specifier returns date in MM/DD/YY format
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = strftime("%D");
// %D should return MM/DD/YY format, so length should be 8 (MM/DD/YY)
if (is_string($result) && strlen($result) === 8 && strpos($result, '/') !== false) {
    echo "PASS\n";
} else {
    echo "FAIL: expected MM/DD/YY format, got " . var_export($result, true) . "\n";
}
?>
--EXPECT--
PASS