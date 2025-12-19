--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array walk with callback functions
--FILE--
<?php
// Test array_walk with various callback scenarios
$a = array("key1" => "value1", "key2" => "value2", "key3" => "value3");

// Test array_walk with simple callback
function simple_callback($value, $key) {
    echo "Key: $key, Value: $value\n";
}

$result = array_walk($a, 'simple_callback');
echo "Simple callback result: " . ($result ? "true" : "false") . "\n";

// Test array_walk with closure
$result2 = array_walk($a, function($value, $key) {
    // Do nothing, just test closure
});
echo "Closure callback result: " . ($result2 ? "true" : "false") . "\n";

// Test array_walk with empty array
$empty = array();
$result3 = array_walk($empty, 'simple_callback');
echo "Empty array walk result: " . ($result3 ? "true" : "false") . "\n";

echo "walk_callback_ok\n";
?>
--EXPECT--
Key: key1, Value: value1
Key: key2, Value: value2
Key: key3, Value: value3
Simple callback result: true
Closure callback result: true
Empty array walk result: true
walk_callback_ok