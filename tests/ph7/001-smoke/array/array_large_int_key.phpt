--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with very large integer keys
--FILE--
<?php
// Test array operations with very large integer keys
// This tests hashmap bucket growth paths at lines 214-215 in hashmap.c
$array = array();

// Insert very large integer keys
$array[999999999] = "large_key_1";
$array[2147483647] = "max_int_32";
$array[-2147483648] = "min_int_32";
$array[123456789012345] = "very_large";

// Access
echo "999999999: " . $array[999999999] . "\n";
echo "max_int_32: " . $array[2147483647] . "\n";
echo "min_int_32: " . $array[-2147483648] . "\n";
echo "very_large: " . $array[123456789012345] . "\n";

// Verify count
echo "count: " . count($array) . "\n";
?>
--EXPECT--
999999999: large_key_1
max_int_32: max_int_32
min_int_32: min_int_32
very_large: very_large
count: 4
--CLEAN--
<?php
unset($array);
