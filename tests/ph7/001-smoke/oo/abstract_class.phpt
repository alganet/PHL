--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Abstract class test
--FILE--
<?php
abstract class AbstractBase {
    abstract public function doSomething();
    
    public function concreteMethod() {
        return "concrete";
    }
}

class ConcreteClass extends AbstractBase {
    public function doSomething() {
        return "implemented";
    }
}

$instance = new ConcreteClass();
echo "Concrete method: " . $instance->concreteMethod() . "\n";
echo "Abstract method: " . $instance->doSomething() . "\n";
?>
--EXPECT--
Concrete method: concrete
Abstract method: implemented
--CLEAN--
<?php
// No cleanup needed
unset($instance);
