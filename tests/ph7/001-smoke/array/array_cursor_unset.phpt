--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array cursor unset operation
--FILE--
<?php
// Test array cursor operations to cover hashmap edge cases
$arr = array('a' => 1, 'b' => 2, 'c' => 3);

// Move cursor to 'b'
reset($arr);
next($arr);

$current = current($arr);
echo "Current before unset: " . $current . "\n";

// Unset the current element
unset($arr['b']);

echo "Array after unset: ";
foreach ($arr as $key => $value) {
    echo "$key => $value ";
}
echo "\n";

echo "Current after unset: " . current($arr) . "\n";
?>
--EXPECT--
Current before unset: 2
Array after unset: a => 1 c => 3 
Current after unset: 3
--CLEAN--
<?php
unset($arr, $current);
