--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: substr_compare length bounds checking
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test length that exceeds remaining string after offset
$result = substr_compare('abc', 'def', 1, 10);
echo "Length exceeds remaining: " . ($result === -2 ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Length exceeds remaining: PASS
--CLEAN--
<?php
unset($result);
