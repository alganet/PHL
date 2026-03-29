--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait property conflict with different defaults produces error
--FILE--
<?php
trait A {
    public $x = 1;
}
trait B {
    public $x = 2;
}
class C {
    use A, B;
}
?>
--EXPECTF--
%s %s %s  A and B define the same property ($x) in the composition of C. However, the definition differs %s
--CLEAN--
<?php
