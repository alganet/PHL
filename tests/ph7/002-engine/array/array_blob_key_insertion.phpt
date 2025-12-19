--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with string (blob) keys
--FILE--
<?php
// Test array operations with string keys to cover hashmap.c blob key insertion path
$array = array();

// Insert string keys
$array["key1"] = "value1";
$array["key2"] = 42;
$array["key3"] = array(1, 2, 3);
$array["another_key"] = "test";

// Access and modify
echo "key1: " . $array["key1"] . "\n";
echo "key2: " . $array["key2"] . "\n";
echo "key3 count: " . count($array["key3"]) . "\n";

// Test isset and array_key_exists
echo "isset key1: " . (isset($array["key1"]) ? "true" : "false") . "\n";
echo "array_key_exists key2: " . (array_key_exists("key2", $array) ? "true" : "false") . "\n";

// Test with special characters in keys
$array["key-with-dash"] = "special";
$array["key_with_underscore"] = "normal";
$array["123numeric"] = "starts_with_number";

echo "Special key: " . $array["key-with-dash"] . "\n";
echo "Numeric key: " . $array["123numeric"] . "\n";

// Test array_keys and array_values
$keys = array_keys($array);
$values = array_values($array);
echo "Number of keys: " . count($keys) . "\n";
echo "Number of values: " . count($values) . "\n";
?>
--EXPECT--
key1: value1
key2: 42
key3 count: 3
isset key1: true
array_key_exists key2: true
Special key: special
Numeric key: starts_with_number
Number of keys: 7
Number of values: 7