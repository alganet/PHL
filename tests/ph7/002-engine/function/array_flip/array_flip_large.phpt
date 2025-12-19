--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip with large array to exercise hashmap operations
--FILE--
<?php
// Create a large array with many unique keys and values
$array = array();
for ($i = 0; $i < 1000; $i++) {
    $array["key_$i"] = "value_$i";
}

// Flip the array
$flipped = array_flip($array);

// Verify the flip worked
$success = true;
for ($i = 0; $i < 1000; $i++) {
    if (!isset($flipped["value_$i"]) || $flipped["value_$i"] !== "key_$i") {
        $success = false;
        break;
    }
}

echo $success ? "PASS\n" : "FAIL\n";
echo "Original count: " . count($array) . "\n";
echo "Flipped count: " . count($flipped) . "\n";
?>
--EXPECT--
PASS
Original count: 1000
Flipped count: 1000