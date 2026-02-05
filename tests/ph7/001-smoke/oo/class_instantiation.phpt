--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO class instantiation and basic operations
--FILE--
<?php
// Test basic class definition and instantiation
class ClassInstantiationTestClass {
    public $public_attr;
    private $private_attr;
}

echo "Class defined successfully\n";

// Test instantiation
$obj = new ClassInstantiationTestClass();
echo "Object created: " . (is_object($obj) ? "yes" : "no") . "\n";

// Test instanceof
$is_instance = $obj instanceof ClassInstantiationTestClass;
echo "Instanceof check: " . ($is_instance ? "true" : "false") . "\n";

// Test attribute access (public attributes should work)
$obj->public_attr = "test_value";
echo "Public attribute set: " . $obj->public_attr . "\n";

// Test get_class
$class_name = get_class($obj);
echo "Class name: " . $class_name . "\n";
?>
--EXPECTF--
Class defined successfully
Object created: yes
Instanceof check: true
Public attribute set: test_value
Class name: ClassInstantiationTestClass
--CLEAN--
<?php
unset($obj, $is_instance, $class_name);
