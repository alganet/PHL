--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multiple traits in single use statement
--FILE--
<?php
trait A {
    public function a() { return "A"; }
}
trait B {
    public function b() { return "B"; }
}
class C {
    use A, B;
}
$c = new C();
echo $c->a(), "\n";
echo $c->b(), "\n";
?>
--EXPECT--
A
B
--CLEAN--
<?php
