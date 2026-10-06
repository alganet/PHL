--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A private(set) property is final without saying so
--DESCRIPTION--
No subclass may redeclare it, even with the same modifiers, and a promoted
constructor parameter is a redeclaration too.
--FILE--
<?php
class PpsfParent {
    public private(set) int $p = 1;
}
class PpsfKid extends PpsfParent {
    public function __construct(public int $p = 2) {}
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot override final property PpsfParent::$p %s
