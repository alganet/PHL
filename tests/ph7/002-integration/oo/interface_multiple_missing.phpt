--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class missing multiple interface methods lists them all
--FILE--
<?php
interface I {
    public function a();
    public function b();
}
class C implements I {
}
?>
--EXPECTF--
%s %s %s  Class C contains 2 abstract methods and must therefore be declared abstract or implement the remaining methods (I::a, I::b) %s
--CLEAN--
<?php
