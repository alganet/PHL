--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class introspection functions test
--FILE--
<?php
class ClassIntrospectTestClass {
    public $publicProp = 'public';
    private $privateProp = 'private';
    
    public function publicMethod() {}
    private function privateMethod() {}
}

$obj = new ClassIntrospectTestClass();

echo "class_exists('ClassIntrospectTestClass'): " . (class_exists('ClassIntrospectTestClass') ? 'true' : 'false') . "\n";
echo "class_exists('NonExistentClass'): " . (class_exists('NonExistentClass') ? 'true' : 'false') . "\n";
echo "method_exists('ClassIntrospectTestClass', 'publicMethod'): " . (method_exists('ClassIntrospectTestClass', 'publicMethod') ? 'true' : 'false') . "\n";
echo "method_exists('ClassIntrospectTestClass', 'nonExistentMethod'): " . (method_exists('ClassIntrospectTestClass', 'nonExistentMethod') ? 'true' : 'false') . "\n";
echo "property_exists('ClassIntrospectTestClass', 'publicProp'): " . (property_exists('ClassIntrospectTestClass', 'publicProp') ? 'true' : 'false') . "\n";
echo "property_exists('ClassIntrospectTestClass', 'nonExistentProp'): " . (property_exists('ClassIntrospectTestClass', 'nonExistentProp') ? 'true' : 'false') . "\n";
echo "property_exists(\$obj, 'publicProp'): " . (property_exists($obj, 'publicProp') ? 'true' : 'false') . "\n";
?>
--EXPECT--
class_exists('ClassIntrospectTestClass'): true
class_exists('NonExistentClass'): false
method_exists('ClassIntrospectTestClass', 'publicMethod'): true
method_exists('ClassIntrospectTestClass', 'nonExistentMethod'): false
property_exists('ClassIntrospectTestClass', 'publicProp'): true
property_exists('ClassIntrospectTestClass', 'nonExistentProp'): false
property_exists($obj, 'publicProp'): true
--CLEAN--
<?php
// No cleanup needed
?>
