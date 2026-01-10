--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with very large integer key
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array with very large integer key
$large_key = 9223372036854775807; // PHP_INT_MAX
$arr = array();
$arr[$large_key] = "large_value";
echo "Key exists: " . (isset($arr[$large_key]) ? "yes" : "no") . "\n";
echo "Value: " . $arr[$large_key] . "\n";

// Test negative large key
$neg_large_key = -9223372036854775808; // PHP_INT_MIN
$arr[$neg_large_key] = "neg_large_value";
echo "Negative key exists: " . (isset($arr[$neg_large_key]) ? "yes" : "no") . "\n";
echo "Negative value: " . $arr[$neg_large_key] . "\n";
?>
--EXPECT--
Key exists: yes
Value: large_value
Negative key exists: yes
Negative value: neg_large_value