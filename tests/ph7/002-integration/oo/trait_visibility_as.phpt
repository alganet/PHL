--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait visibility override with as
--FILE--
<?php
trait HelloWorld {
    public function sayHello() {
        echo "Hello World\n";
    }
}
class MyClass {
    use HelloWorld {
        sayHello as protected;
    }
}
class SubClass extends MyClass {
    public function test() {
        $this->sayHello();
    }
}
$sub = new SubClass();
$sub->test();
?>
--EXPECT--
Hello World
--CLEAN--
<?php
