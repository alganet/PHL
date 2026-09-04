--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations under allocation stress

--FILE--
<?php
// Test allocation failure paths in hashmap operations
// This should exercise HashmapNewIntNode allocation failure handling

// Create a large array to stress the hashmap
$large_array = array();
for ($i = 0; $i < 10000; $i++) {
    $large_array[$i] = "value_" . $i;
}

// Test various operations that allocate hashmap nodes
$result1 = array_flip($large_array);
echo "Flip operation completed\n";

$result2 = array_merge($large_array, array("extra" => "value"));
echo "Merge operation completed\n";

// Test with mixed key types
$mixed_keys = array();
$mixed_keys[0] = "zero";
$mixed_keys["string"] = "string_value";
$mixed_keys[1.5] = "float_key";
$mixed_keys[true] = "bool_key";

$result3 = array_keys($mixed_keys);
echo "Mixed keys operation completed\n";

echo "All allocation stress tests passed\n";
?>
--EXPECTF--
Flip operation completed
Merge operation completed
Error [8192]: Implicit conversion from float 1.5 to int loses precision in %s on line %d
Mixed keys operation completed
All allocation stress tests passed
--CLEAN--
<?php
unset($large_array, $result1, $result2, $mixed_keys, $result3);
