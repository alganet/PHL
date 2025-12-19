--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk with callback that modifies array elements
--FILE--
<?php
function modify_callback(&$value, $key) {
    $value = $value * 2; // Double the value
    echo "Modified $key to $value\n";
}

$array = array('a' => 1, 'b' => 2, 'c' => 3);
$result = array_walk($array, 'modify_callback');
echo "array_walk result: " . ($result ? "PASS" : "FAIL") . "\n";
echo "Final array: " . implode(', ', $array) . "\n";
?>
--EXPECT--
Modified a to 2
Modified b to 4
Modified c to 6
array_walk result: PASS
Final array: 2, 4, 6