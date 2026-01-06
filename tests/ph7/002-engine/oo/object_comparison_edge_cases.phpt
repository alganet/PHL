--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison edge cases and complex comparison logic
--FILE--
<?php
// Test object comparison edge cases that exercise OO comparison logic
// This covers various object comparison scenarios

class TestClass {
    public $value;
    public $name;

    public function __construct($value, $name = 'default') {
        $this->value = $value;
        $this->name = $name;
    }
}

class OtherClass {
    public $value;

    public function __construct($value) {
        $this->value = $value;
    }
}

// Test basic equality
$obj1 = new TestClass(5, 'test');
$obj2 = new TestClass(5, 'test');
$obj3 = new TestClass(5, 'different');

echo "Same values, same class: " . (($obj1 == $obj2) ? 'equal' : 'not equal') . "\n";
echo "Same values, different names: " . (($obj1 == $obj3) ? 'equal' : 'not equal') . "\n";

// Test identity (===)
echo "Identity check: " . (($obj1 === $obj1) ? 'same instance' : 'different instance') . "\n";
echo "Identity check different objects: " . (($obj1 === $obj2) ? 'same instance' : 'different instance') . "\n";

// Test with different classes
$other = new OtherClass(5);
echo "Different classes same value: " . (($obj1 == $other) ? 'equal' : 'not equal') . "\n";

// Test with null values
class NullTestClass {
    public $val1 = null;
    public $val2 = null;
}

$null1 = new NullTestClass();
$null2 = new NullTestClass();
echo "Null values comparison: " . (($null1 == $null2) ? 'equal' : 'not equal') . "\n";

// Test with arrays in objects
class ArrayTestClass {
    public $arr;

    public function __construct($arr) {
        $this->arr = $arr;
    }
}

$arrObj1 = new ArrayTestClass(array(1, 2, 3));
$arrObj2 = new ArrayTestClass(array(1, 2, 3));
echo "Array comparison: " . (($arrObj1 == $arrObj2) ? 'equal' : 'not equal') . "\n";
?>
--EXPECT--
Same values, same class: equal
Same values, different names: not equal
Identity check: same instance
Identity check different objects: different instance
Different classes same value: not equal
Null values comparison: equal
Array comparison: equal