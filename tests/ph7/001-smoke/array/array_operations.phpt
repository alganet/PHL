--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations edge cases
--SKIPIF--
<?php echo 'skip'; ?>
--FILE--
<?php
// Test array operations that exercise edge cases in hashmap.c

// Test array_flip with various value types
echo "Testing array_flip:\n";
$flip1 = array("a" => 1, "b" => 2, "c" => 1); // Duplicate values
$result1 = array_flip($flip1);
print_r($result1);

// Test array_flip with different types
$flip2 = array(1 => "string", 2 => 2.5, 3 => true);
$result2 = array_flip($flip2);
print_r($result2);

// Test array_search with strict comparison
echo "\nTesting array_search:\n";
$search = array(1, "1", true, false, null);
echo "array_search(1, \$search, false): " . array_search(1, $search, false) . "\n";
echo "array_search(1, \$search, true): " . array_search(1, $search, true) . "\n";
echo "array_search('1', \$search, false): " . array_search('1', $search, false) . "\n";
echo "array_search('1', \$search, true): " . array_search('1', $search, true) . "\n";

// Test in_array with strict comparison
echo "\nTesting in_array:\n";
echo "in_array(0, \$search, false): " . (in_array(0, $search, false) ? 'true' : 'false') . "\n";
echo "in_array(0, \$search, true): " . (in_array(0, $search, true) ? 'true' : 'false') . "\n";
echo "in_array(false, \$search, false): " . (in_array(false, $search, false) ? 'true' : 'false') . "\n";
echo "in_array(false, \$search, true): " . (in_array(false, $search, true) ? 'true' : 'false') . "\n";

// Test array_walk with callback
echo "\nTesting array_walk:\n";
$walk_array = array("a" => 1, "b" => 2, "c" => 3);
function test_callback($value, $key) {
    echo "Key: $key, Value: $value\n";
}
array_walk($walk_array, 'test_callback');

// Test nested arrays and count operations
echo "\nTesting nested count:\n";
$nested = array(
    "level1" => array("a", "b", "c"),
    "level2" => array("d" => array(1, 2), "e" => array(3, 4, 5))
);
echo "count(\$nested, COUNT_NORMAL): " . count($nested, COUNT_NORMAL) . "\n";
echo "count(\$nested, COUNT_RECURSIVE): " . count($nested, COUNT_RECURSIVE) . "\n";

// Test array with large keys (exercise bucket growth)
echo "\nTesting large keys:\n";
$large_key_array = array();
for ($i = 0; $i < 100; $i++) {
    $large_key_array[$i * 1000] = "value_$i";
}
echo "Large key array count: " . count($large_key_array) . "\n";
echo "Sample large key value: " . $large_key_array[50000] . "\n";

echo "Array operations test completed\n";
?>
--EXPECT--
Testing array_flip:
Array(2) {
 [1] =>
  c
 [2] =>
  b
 }
Array(3) {
 [string] =>
  1
 [2] =>
  2
 [1] =>
  3
 }

Testing array_search:
array_search(1, $search, false): 0
array_search(1, $search, true): 0
array_search('1', $search, false): 0
array_search('1', $search, true): 1

Testing in_array:
in_array(0, $search, false): true
in_array(0, $search, true): false
in_array(false, $search, false): true
in_array(false, $search, true): true

Testing array_walk:
Key: a, Value: 1
Key: b, Value: 2
Key: c, Value: 3

Testing nested count:
count($nested, COUNT_NORMAL): 2
count($nested, COUNT_RECURSIVE): 12

Testing large keys:
Large key array count: 100
Sample large key value: value_50

Array operations test completed
--CLEAN--
<?php
unset($flip1, $result1, $flip2, $result2, $search, $walk_array, $nested, $large_key_array);
