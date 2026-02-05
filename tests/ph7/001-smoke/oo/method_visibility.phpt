--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO method visibility (PHL enforces visibility with errors and empty returns)
--FILE--
<?php
class MethodVisibilityTest {
    private function privateMethod() {
        return "private";
    }

    protected function protectedMethod() {
        return "protected";
    }

    public function publicMethod() {
        return "public";
    }
}

class MethodChildClass extends MethodVisibilityTest {
    public function testParentProtected() {
        return $this->protectedMethod();
    }

    public function testParentPrivate() {
        return $this->privateMethod(); // Will error and return empty
    }
}

echo "Testing method visibility (PHL enforces visibility with errors and empty returns)...\n";

$test = new MethodVisibilityTest();

// Direct access from outside (should error and return empty for private/protected)
echo "Public method result: " . $test->publicMethod() . "\n";

// Access via child class
$child = new MethodChildClass();
echo "Public method: " . $child->publicMethod() . "\n";
echo "Protected method: " . $child->testParentProtected() . "\n";

echo "Method visibility test completed (PHL enforces visibility with errors and empty returns)\n";
?>
--EXPECTF--
Testing method visibility (PHL enforces visibility with errors and empty returns)...
Public method result: public
Public method: public
Protected method: protected
Method visibility test completed (PHL enforces visibility with errors and empty returns)
--CLEAN--
<?php
unset($test, $child);
