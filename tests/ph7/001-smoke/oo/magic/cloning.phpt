--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object cloning test
--FILE--
<?php
class CloningTestClass {
    public $value;
    public $array;
    
    public function __construct($val) {
        $this->value = $val;
        $this->array = array(1, 2, 3);
    }
    
    public function __clone() {
        $this->value *= 2;
        $this->array = array_reverse($this->array);
    }
}

$original = new CloningTestClass(10);
echo "Original value: " . $original->value . "\n";
echo "Original array: " . implode(',', $original->array) . "\n";

$clone = clone $original;
echo "Clone value: " . $clone->value . "\n";
echo "Clone array: " . implode(',', $clone->array) . "\n";

// Modify original to ensure clone is separate
$original->value = 99;
$original->array[0] = 999;

echo "After modification - Original value: " . $original->value . "\n";
echo "After modification - Clone value: " . $clone->value . "\n";
echo "After modification - Original array: " . implode(',', $original->array) . "\n";
echo "After modification - Clone array: " . implode(',', $clone->array) . "\n";
?>
--EXPECT--
Original value: 10
Original array: 1,2,3
Clone value: 20
Clone array: 3,2,1
After modification - Original value: 99
After modification - Clone value: 20
After modification - Original array: 999,2,3
After modification - Clone array: 3,2,1
--CLEAN--
<?php
unset($original, $clone);
