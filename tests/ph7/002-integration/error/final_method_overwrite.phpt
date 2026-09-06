--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Cannot overwrite final method in subclass
--FILE--
<?php
class A {
    final public function test() { }
}
class B extends A {
    public function test() { }
}
?>
--EXPECTF--
%AFatal error:%ACannot override final method A::test()%A
--CLEAN--
<?php

