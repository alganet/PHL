--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object property access and walk functionality
--FILE--
<?php
class TestClass {
    public $public_var = 'public_value';
    protected $protected_var = 'protected_value';
    private $private_var = 'private_value';
    public $another = 'another_value';
}

$obj = new TestClass();

// Access public properties (exercises ph7_object_fetch_attr)
echo $obj->public_var . "\n";
echo $obj->another . "\n";

// Test dynamic property access
$prop = 'public_var';
echo $obj->$prop . "\n";

// Test isset on object properties
echo isset($obj->public_var) ? "isset_ok\n" : "isset_fail\n";

// Test empty on object properties
echo empty($obj->public_var) ? "empty_fail\n" : "empty_ok\n";

// Test unset on object properties
unset($obj->another);
echo isset($obj->another) ? "unset_fail\n" : "unset_ok";
?>
--EXPECT--
public_value
another_value
public_value
isset_ok
empty_ok
unset_ok
--CLEAN--
<?php
unset($obj, $prop);
