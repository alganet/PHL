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

class MagicChildClass extends BaseClass {
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

class GrandMagicChildClass extends MagicChildClass {
}

echo "Testing ::class constant...\n";

// Test with base class
$base = new BaseClass();
echo "Base self::class: " . $base->getClassName() . "\n";
echo "Base static::class: " . $base->getStaticClassName() . "\n";
echo "BaseClass::class: " . BaseClass::class . "\n";

// Test with child class
$child = new MagicChildClass();
echo "Child parent::class: " . $child->getParentClassName() . "\n";
echo "Child self::class: " . $child->getSelfClassName() . "\n";
echo "Child static::class: " . $child->getStaticClassName() . "\n";
echo "MagicChildClass::class: " . MagicChildClass::class . "\n";

// Test static::class with inheritance
$grand = new GrandMagicChildClass();
echo "GrandChild static::class (from Base): " . $grand->getStaticClassName() . "\n";
echo "GrandChild self::class (from Child): " . $grand->getSelfClassName() . "\n";
echo "GrandMagicChildClass::class: " . GrandMagicChildClass::class . "\n";

// Test with direct class reference (no instance)
echo "Direct class reference tests:\n";
echo "BaseClass::class = " . BaseClass::class . "\n";
echo "MagicChildClass::class = " . MagicChildClass::class . "\n";
echo "GrandMagicChildClass::class = " . GrandMagicChildClass::class . "\n";

echo "::class constant test completed\n";
?>
--EXPECTF--
Testing ::class constant...
Base self::class: BaseClass
Base static::class: BaseClass
BaseClass::class: BaseClass
Child parent::class: BaseClass
Child self::class: MagicChildClass
Child static::class: MagicChildClass
MagicChildClass::class: MagicChildClass
GrandChild static::class (from Base): GrandMagicChildClass
GrandChild self::class (from Child): MagicChildClass
GrandMagicChildClass::class: GrandMagicChildClass
Direct class reference tests:
BaseClass::class = BaseClass
MagicChildClass::class = MagicChildClass
GrandMagicChildClass::class = GrandMagicChildClass
::class constant test completed
--CLEAN--
<?php
unset($base, $child, $grand);
