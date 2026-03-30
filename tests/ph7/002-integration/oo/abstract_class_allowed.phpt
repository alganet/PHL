--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Abstract class is allowed to have unimplemented abstract methods
--FILE--
<?php
interface I {
    public function a();
    public function b();
}
abstract class Base implements I {
    public function a() { return "a"; }
}
class Child extends Base {
    public function b() { return "b"; }
}
$c = new Child();
echo $c->a(), "\n";
echo $c->b(), "\n";
?>
--EXPECT--
a
b
--CLEAN--
<?php
