--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_key_exists with non-array first argument
$result1 = array_key_exists("key", "not an array");
echo "Non-array: " . ($result1 === false ? "PASS" : "FAIL") . "\n";

// Test array_key_exists with too few arguments
$result2 = array_key_exists("key");
echo "Too few args: " . ($result2 === false ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Non-array: PASS
Too few args: PASS
--CLEAN--
<?php
unset($result1, $result2);
