--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object cloning without __clone method
--FILE--
<?php
class SimpleClass {
    public $value = 42;
    public $str = "test";
}

$original = new SimpleClass();
echo "Original value: " . $original->value . "\n";
echo "Original str: " . $original->str . "\n";

$clone = clone $original;
echo "Clone value: " . $clone->value . "\n";
echo "Clone str: " . $clone->str . "\n";

// Verify they are separate objects
$original->value = 99;
$original->str = "modified";

echo "After modification - Original value: " . $original->value . "\n";
echo "After modification - Clone value: " . $clone->value . "\n";
echo "After modification - Original str: " . $original->str . "\n";
echo "After modification - Clone str: " . $clone->str . "\n";
?>
--EXPECT--
Original value: 42
Original str: test
Clone value: 42
Clone str: test
After modification - Original value: 99
After modification - Clone value: 42
After modification - Original str: modified
After modification - Clone str: test
--CLEAN--
<?php
unset($original, $clone);
