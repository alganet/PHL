--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr with invalid offsets
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test substr with offset >= string length
$result = substr("hello", 10);
if ($result === false) {
    echo "PASS1\n";
} else {
    echo "FAIL1\n";
}

// Test substr with very negative offset
$result = substr("hello", -10);
if ($result === false) {
    echo "PASS2\n";
} else {
    echo "FAIL2\n";
}
?>
--EXPECT--
PASS1
PASS2
--CLEAN--
<?php
unset($result);
