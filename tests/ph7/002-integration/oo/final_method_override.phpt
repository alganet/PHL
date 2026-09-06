--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Cannot overwrite a final method in subclass
--FILE--
<?php
class A {
    final public function f() { return 1; }
}
class B extends A {
    public function f() { return 2; }
}
?>
--EXPECTF--
%AFatal error:%ACannot override final method A::f()%A
--CLEAN--
<?php

