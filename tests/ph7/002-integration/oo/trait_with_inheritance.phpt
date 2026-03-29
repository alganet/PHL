--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait methods accessible through class inheritance
--FILE--
<?php
trait Greet {
    public function hello() { return "Hello"; }
}
class Base {
    use Greet;
}
class Child extends Base {
    public function test() {
        return $this->hello() . " World";
    }
}
$c = new Child();
echo $c->hello(), "\n";
echo $c->test(), "\n";
?>
--EXPECT--
Hello
Hello World
--CLEAN--
<?php
