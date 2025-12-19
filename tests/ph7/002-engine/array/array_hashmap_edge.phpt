--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array hashmap edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array operations that may trigger hashmap edge cases
$array = array();

// Test with nested arrays and recursive operations
$nested = array(
    'a' => array('b' => array('c' => 1)),
    'd' => array('e' => array('f' => 2))
);

$count = count($nested, COUNT_RECURSIVE);
echo "Recursive count: $count\n";

// Test array with mixed types
$mixed = array(
    0 => 'zero',
    '0' => 'string_zero',
    1.5 => 'float_key',
    true => 'bool_key'
);

$keys = array_keys($mixed);
echo "Keys count: " . count($keys) . "\n";

echo "Test completed\n";
?>
--EXPECT--
Recursive count: 6
Keys count: 2
Test completed