--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO ::class constant (PHP 5.5+)
--FILE--
<?php
class BaseClass {
    public function getClassName() {
        return self::class;
    }

    public function getStaticClassName() {
        return static::class;
    }
}

class ChildClass extends BaseClass {
    public function getParentClassName() {
        return parent::class;
    }

    public function getSelfClassName() {
        return self::class;
    }

    public function getStaticClassName() {
        return static::class;
    }
}

class GrandChildClass extends ChildClass {
}

echo "Testing ::class constant...\n";

// Test with base class
$base = new BaseClass();
echo "Base self::class: " . $base->getClassName() . "\n";
echo "Base static::class: " . $base->getStaticClassName() . "\n";
echo "BaseClass::class: " . BaseClass::class . "\n";

// Test with child class
$child = new ChildClass();
echo "Child parent::class: " . $child->getParentClassName() . "\n";
echo "Child self::class: " . $child->getSelfClassName() . "\n";
echo "Child static::class: " . $child->getStaticClassName() . "\n";
echo "ChildClass::class: " . ChildClass::class . "\n";

// Test static::class with inheritance
$grand = new GrandChildClass();
echo "GrandChild static::class (from Base): " . $grand->getStaticClassName() . "\n";
echo "GrandChild self::class (from Child): " . $grand->getSelfClassName() . "\n";
echo "GrandChildClass::class: " . GrandChildClass::class . "\n";

// Test with direct class reference (no instance)
echo "Direct class reference tests:\n";
echo "BaseClass::class = " . BaseClass::class . "\n";
echo "ChildClass::class = " . ChildClass::class . "\n";
echo "GrandChildClass::class = " . GrandChildClass::class . "\n";

echo "::class constant test completed\n";
?>
--EXPECTF--
Testing ::class constant...
Base self::class: BaseClass
Base static::class: BaseClass
BaseClass::class: BaseClass
Child parent::class: BaseClass
Child self::class: ChildClass
Child static::class: ChildClass
ChildClass::class: ChildClass
GrandChild static::class (from Base): GrandChildClass
GrandChild self::class (from Child): ChildClass
GrandChildClass::class: GrandChildClass
Direct class reference tests:
BaseClass::class = BaseClass
ChildClass::class = ChildClass
GrandChildClass::class = GrandChildClass
::class constant test completed
--CLEAN--
<?php
unset($base, $child, $grand);
