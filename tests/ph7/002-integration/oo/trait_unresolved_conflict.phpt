--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unresolved trait method conflict produces error
--FILE--
<?php
trait A {
    public function hello() { return "A"; }
}
trait B {
    public function hello() { return "B"; }
}
class C {
    use A, B;
}
?>
--EXPECTF--
%s %s %s  Trait method B::hello has not been applied as C::hello, because of collision with A::hello %s
--CLEAN--
<?php
