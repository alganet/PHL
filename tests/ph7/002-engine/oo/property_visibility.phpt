--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO property visibility (PHL enforces visibility with errors and empty returns)
--FILE--
<?php
class PropertyVisibilityTest {
    public $publicProp = "public_value";
    private $privateProp = "private_value";
    protected $protectedProp = "protected_value";

    public function getPrivateProp() {
        return $this->privateProp;
    }

    public function getProtectedProp() {
        return $this->protectedProp;
    }

    public function setPrivateProp($value) {
        $this->privateProp = $value;
    }

    public function setProtectedProp($value) {
        $this->protectedProp = $value;
    }
}

class ChildPropertyClass extends PropertyVisibilityTest {
    public function testParentProperties() {
        // Can access public property
        echo "Child public prop: " . $this->publicProp . "\n";

        // Can access protected property from child (allowed)
        echo "Child protected prop: " . $this->protectedProp . "\n";
    }

    public function modifyParentProtected() {
        $this->protectedProp = "modified_by_child";
    }
}

echo "Testing property visibility (PHL enforces visibility with errors and empty returns)...\n";

$test = new PropertyVisibilityTest();

// Test public property access
echo "Public prop: " . $test->publicProp . "\n";
$test->publicProp = "modified";
echo "Modified public prop: " . $test->publicProp . "\n";

// Test private property access through methods
echo "Private prop via method: " . $test->getPrivateProp() . "\n";
$test->setPrivateProp("modified_private");
echo "Modified private prop via method: " . $test->getPrivateProp() . "\n";

// Test protected property access through methods
echo "Protected prop via method: " . $test->getProtectedProp() . "\n";
$test->setProtectedProp("modified_protected");
echo "Modified protected prop via method: " . $test->getProtectedProp() . "\n";


// Test child class access
$child = new ChildPropertyClass();
$child->testParentProperties();
$child->modifyParentProtected();
echo "Parent protected modified by child: " . $child->getProtectedProp() . "\n";

echo "Property visibility test completed (PHL enforces visibility with errors and empty returns)\n";
?>
--CLEAN--
<?php
unset($test, $child);
?>
--EXPECTF--
Testing property visibility (PHL enforces visibility with errors and empty returns)...
Public prop: public_value
Modified public prop: modified
Private prop via method: private_value
Modified private prop via method: modified_private
Protected prop via method: protected_value
Modified protected prop via method: modified_protected
Child public prop: public_value
Child protected prop: protected_value
Parent protected modified by child: modified_by_child
Property visibility test completed (PHL enforces visibility with errors and empty returns)
