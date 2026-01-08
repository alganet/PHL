--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_count with needle longer than haystack returns 0
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test substr_count with needle longer than haystack
$result = substr_count("a", "aa");
if ($result === 0) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS