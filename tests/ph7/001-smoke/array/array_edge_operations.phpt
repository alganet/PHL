--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array edge operations
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array operations that might trigger edge cases in hashmap
$array = array();

// Test empty array operations
var_dump(count($array)); // Should be 0
var_dump(empty($array)); // Should be true

// Test array with mixed keys
$array[0] = "zero";
$array["1"] = "one";
$array[2.5] = "two point five";
$array[true] = "true";
$array[false] = "false";

var_dump(count($array)); // Should be 5
var_dump(isset($array[0])); // Should be true
var_dump(isset($array["1"])); // Should be true

// Test array_flip edge cases
$flip_array = array("a" => 1, "b" => 2, "c" => 1); // Duplicate values
$flipped = array_flip($flip_array);
var_dump(count($flipped)); // Should handle duplicates

// Test large key
$large_key = str_repeat("x", 100);
$array[$large_key] = "large";
var_dump(isset($array[$large_key])); // Should be true
?>
--EXPECT--
int(0)
bool(true)
int(3)
bool(true)
bool(true)
int(2)
bool(true)
--CLEAN--
<?php
unset($array, $flip_array, $flipped, $large_key);
