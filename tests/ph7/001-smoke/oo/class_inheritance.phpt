--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO class inheritance
--FILE--
<?php
class OoParentClass {
    public $parent_attr = "parent_value";
}

class OoChildClass extends OoParentClass {
    public $child_attr = "child_value";
}

echo "Classes defined successfully\n";

// Test parent class instantiation
$parent = new OoParentClass();
echo "Parent attr: " . $parent->parent_attr . "\n";

// Test child class instantiation
$child = new OoChildClass();
echo "Child parent attr: " . $child->parent_attr . "\n";
echo "Child child attr: " . $child->child_attr . "\n";

// Test instanceof with inheritance
$is_child_of_parent = $child instanceof OoParentClass;
$is_parent_of_child = $parent instanceof OoChildClass;
echo "Child instanceof Parent: " . ($is_child_of_parent ? "true" : "false") . "\n";
echo "Parent instanceof Child: " . ($is_parent_of_child ? "true" : "false") . "\n";

// Test get_class on child
$child_class_name = get_class($child);
echo "Child class name: " . $child_class_name . "\n";

// Test is_subclass_of
$is_subclass = is_subclass_of($child, 'OoParentClass');
echo "Is subclass: " . ($is_subclass ? "true" : "false") . "\n";
?>
--EXPECTF--
Classes defined successfully
Parent attr: parent_value
Child parent attr: parent_value
Child child attr: child_value
Child instanceof Parent: true
Parent instanceof Child: false
Child class name: OoChildClass
Is subclass: true
--CLEAN--
<?php
unset($parent, $child, $is_child_of_parent, $is_parent_of_child, $child_class_name, $is_subclass);
