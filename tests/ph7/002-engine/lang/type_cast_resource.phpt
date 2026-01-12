--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Type casting with resource values
--FILE--
<?php
// Test type casting of resource values to various types

// Create a resource (file handle)
$fp = fopen(__FILE__, 'r');
if (!$fp) {
    echo "Failed to open file\n";
    exit(1);
}

// Test casting resource to int
$int_val = (int)$fp;
echo "resource to int: " . ($int_val === 1 ? 'ok' : 'fail') . "\n";

// Test casting resource to float
$float_val = (float)$fp;
echo "resource to float: " . ($float_val == 1.0 ? 'ok' : 'fail') . "\n";

// Test casting resource to bool
$bool_val = (bool)$fp;
echo "resource to bool: " . ($bool_val === true ? 'ok' : 'fail') . "\n";

// Test casting resource to string
$str_val = (string)$fp;
echo "resource to string contains ResourceID_: " . (strpos($str_val, 'ResourceID_') === 0 ? 'ok' : 'fail') . "\n";

// Test casting resource to array
$arr_val = (array)$fp;
echo "resource to array: " . (is_array($arr_val) ? 'ok' : 'fail') . "\n";

// Test casting resource to object
$obj_val = (object)$fp;
echo "resource to object: " . (is_object($obj_val) ? 'ok' : 'fail') . "\n";

fclose($fp);
?>
--EXPECT--
resource to int: ok
resource to float: ok
resource to bool: ok
resource to string contains ResourceID_: ok
resource to array: ok
resource to object: ok