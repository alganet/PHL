--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test class name duplication handling
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test class name duplication scenarios that may exercise VM class registration code
class TestClass {
    public function hello() {
        return "hello";
    }
}

$obj1 = new TestClass();
echo $obj1->hello() . "\n";

// Redefine the same class (may trigger duplication handling in VM)
eval('class TestClass { public function world() { return "world"; } }');

$obj2 = new TestClass();
echo $obj2->world() . "\n";
?>
--EXPECT--
hello
world