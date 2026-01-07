--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation with object member and static access operators
--FILE--
<?php
class Counter {
    public static $count = 42;
    public $value = 100;
    public $items = array('a', 'b', 'c');
}

$obj = new Counter();
$class = 'Counter';

// Test -> member access in string interpolation
echo "Value: {$obj->value}\n";

// Test :: static access in string interpolation  
echo "Count: {$class::$count}\n";

// Test with array access after member
echo "Item: {$obj->items[1]}\n";
?>
--EXPECT--
Value: 100
Count: 42
Item: b