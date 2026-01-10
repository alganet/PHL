--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations edge cases for hashmap coverage
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array operations that exercise hashmap edge cases
// to cover bucket growth and collision handling in hashmap.c

// Test 1: Large array with mixed key types to trigger bucket growth
$large_array = array();
for ($i = 0; $i < 1000; $i++) {
    $large_array["string_key_$i"] = "value_$i";
    $large_array[$i] = "int_value_$i";
    $large_array["collision_" . ($i * 31)] = "collision_value_$i"; // Potential hash collisions
}
echo "Large array created with " . count($large_array) . " elements\n";

// Test 2: Frequent insertions and deletions to trigger rehashing
$test_array = array();
for ($i = 0; $i < 200; $i++) {
    $test_array["key_$i"] = $i;
    if ($i % 10 == 0) {
        // Delete some entries to create holes
        for ($j = max(0, $i - 50); $j < $i; $j += 5) {
            unset($test_array["key_$j"]);
        }
    }
}
echo "Test array after insertions/deletions: " . count($test_array) . " elements\n";

// Test 3: Array with null and empty string keys
$edge_keys = array();
$edge_keys[null] = "null_key";
$edge_keys[""] = "empty_string_key";
$edge_keys[0] = "zero_key";
$edge_keys["0"] = "string_zero_key";
$edge_keys[false] = "false_key";
$edge_keys["false"] = "string_false_key";
echo "Edge keys array: " . count($edge_keys) . " elements\n";

// Test 4: Nested array operations with references
$nested = array();
for ($i = 0; $i < 50; $i++) {
    $nested["level1_$i"] = array(
        "level2_$i" => array("a" => $i, "b" => $i * 2),
        "ref_$i" => &$nested["level1_" . ($i % 10)] // Create circular references
    );
}
echo "Nested array created\n";

// Test 5: Array merge operations that might trigger growth
$array1 = array_fill(0, 100, "value");
$array2 = array_fill(100, 100, "value2");
$merged = array_merge($array1, $array2);
echo "Merged array: " . count($merged) . " elements\n";

// Test 6: Associative array with numeric string keys
$numeric_strings = array();
for ($i = 0; $i < 100; $i++) {
    $numeric_strings["$i"] = "string_$i";
    $numeric_strings[(string)($i + 100)] = "string_" . ($i + 100);
}
echo "Numeric string keys array: " . count($numeric_strings) . " elements\n";

echo "Array edge case operations test completed\n";
?>
--EXPECT--
Large array created with 3000 elements
Test array after insertions/deletions: 162 elements
Edge keys array: 2 elements
Nested array created
Merged array: 200 elements
Numeric string keys array: 200 elements
Array edge case operations test completed