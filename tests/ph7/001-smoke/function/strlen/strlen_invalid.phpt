--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
strlen with invalid arguments
--FILE--
<?php
// Test with no arguments (returns 0 in PH7)
$result = strlen();
echo "No args: " . ($result === 0 ? "PASS" : "FAIL") . "\n";

// Test with NULL (length of empty string)
$result = strlen(NULL);
echo "NULL: " . ($result === 0 ? "PASS" : "FAIL") . "\n";

// Test with integer (converts to string)
$result = strlen(123);
echo "Integer: " . ($result === 3 ? "PASS" : "FAIL") . "\n";

// Test with array (converts to "Array")
$result = strlen(array(1,2,3));
echo "Array: " . ($result === 5 ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
No args: PASS
NULL: PASS
Integer: PASS
Array: PASS
--CLEAN--
<?php
unset($result);
