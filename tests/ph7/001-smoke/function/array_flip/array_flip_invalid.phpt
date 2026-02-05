--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip with invalid argument
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_flip with non-array argument
$result = array_flip("not an array");
echo "Result: " . (is_null($result) ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Result: PASS
--CLEAN--
<?php
unset($result);
