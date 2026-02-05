--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO object attribute access
--FILE--
<?php
class TestObject {
    public $publicAttr = "public_value";
    protected $protectedAttr = "protected_value";
}

$obj = new TestObject();

// Test direct attribute access (should trigger PH7_ClassInstanceFetchAttr)
echo "Public attr: " . $obj->publicAttr . "\n";

// Test accessing protected from within class context
class AccessorTest extends TestObject {
    public function getProtectedAttr() {
        return $this->protectedAttr;
    }
}

$accessor = new AccessorTest();
echo "Protected attr via accessor: " . $accessor->getProtectedAttr() . "\n";

// Test dynamic property access
$propName = "publicAttr";
echo "Dynamic access: " . $obj->$propName . "\n";
?>
--EXPECTF--
Public attr: public_value
Protected attr via accessor: protected_value
Dynamic access: public_value
--CLEAN--
<?php
unset($obj, $accessor, $propName);
