--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with empty arrays
--FILE--
<?php
// Test array operations on empty arrays
$empty = array();

// Test array_flip on empty array
$flipped = array_flip($empty);
echo "Empty array flip: " . (empty($flipped) ? "empty" : "not_empty") . "\n";

// Test array_keys on empty array
$keys = array_keys($empty);
echo "Empty array keys: " . (empty($keys) ? "empty" : "not_empty") . "\n";

// Test array_values on empty array
$values = array_values($empty);
echo "Empty array values: " . (empty($values) ? "empty" : "not_empty") . "\n";

// Test array with valid values
$a = array("key1" => "value1", "key2" => "value2");
$flipped_valid = array_flip($a);
echo "Valid values flip: " . (count($flipped_valid) == 2 ? "flipped" : "not_flipped") . "\n";

echo "empty_operations_ok\n";
?>
--EXPECT--
Empty array flip: empty
Empty array keys: empty
Empty array values: empty
Valid values flip: flipped
empty_operations_ok
--CLEAN--
<?php
unset($empty, $flipped, $keys, $values, $a, $flipped_valid);
