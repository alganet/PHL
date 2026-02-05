--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: static constant in class context returns class name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestClass {
    public function getStatic() {
        return static;
    }
}

$static_outside = static;
$static_inside = (new TestClass())->getStatic();

echo "static outside class: $static_outside\n";
echo "static inside class: $static_inside\n";
?>
--EXPECT--
static outside class: static
static inside class: TestClass
--CLEAN--
<?php
unset($static_outside, $static_inside);
