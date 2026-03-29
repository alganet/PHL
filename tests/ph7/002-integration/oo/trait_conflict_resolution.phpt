--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait conflict resolution with insteadof and as
--FILE--
<?php
trait A {
    public function hello() { return "A::hello"; }
    public function world() { return "A::world"; }
}
trait B {
    public function hello() { return "B::hello"; }
    public function world() { return "B::world"; }
}
class C {
    use A, B {
        A::hello insteadof B;
        B::world insteadof A;
        B::hello as bHello;
    }
}
$c = new C();
echo $c->hello(), "\n";
echo $c->world(), "\n";
echo $c->bHello(), "\n";
?>
--EXPECT--
A::hello
B::world
B::hello
--CLEAN--
<?php
