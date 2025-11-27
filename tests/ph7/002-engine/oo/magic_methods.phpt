--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO magic methods (PHL supports limited magic methods, __destruct not called at script end)
--FILE--
<?php
class MagicMethodsTest {
    // __construct
    public function __construct() {
        echo "__construct called\n";
    }

    // __destruct - NOTE: Not called at script end in PHL
    public function __destruct() {
        echo "__destruct called\n";
    }

    // __toString - called when object is used as string
    public function __toString() {
        return "MagicMethodsTest object";
    }

    // __clone - called when object is cloned
    public function __clone() {
        echo "__clone called\n";
    }

    // Note: __call, __callStatic, __get, __set, __isset, __unset are not supported in PHL
}

echo "Testing magic methods (PHL supports __construct, __destruct, __toString, __clone)...\n";

// Test __construct
$obj = new MagicMethodsTest();

// Test __toString
echo "Object as string: $obj\n";

// Test __clone
$cloned = clone $obj;
echo "Original object: $obj\n";
echo "Cloned object: $cloned\n";

echo "Magic methods test completed\n";

// Manually call destructors since PHL doesn't call them at script end
unset($obj);
unset($cloned);
?>
--CLEAN--
<?php
// Destructors are called here in the CLEAN section
?>
--EXPECTF--
Testing magic methods (PHL supports __construct, __destruct, __toString, __clone)...
__construct called
Object as string: MagicMethodsTest object
__clone called
Original object: MagicMethodsTest object
Cloned object: MagicMethodsTest object
Magic methods test completed
__destruct called
__destruct called
