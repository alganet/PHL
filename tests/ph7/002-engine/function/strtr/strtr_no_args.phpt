--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
strtr with no arguments
--FILE--
<?php
$result = strtr();
if ($result === false) {
    echo "PASS\n";
} else {
    echo "FAIL: expected false, got " . var_export($result, true) . "\n";
}
?>
--EXPECT--
PASS