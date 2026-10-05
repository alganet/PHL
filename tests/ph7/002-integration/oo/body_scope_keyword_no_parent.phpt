--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`parent::` in a method of a class with no base is refused before anything runs
--DESCRIPTION--
A method knows its class while it compiles, and so whether that class extends
anything; php refuses `parent` in one that does not before the program runs. An
enum, an interface and an anonymous class with no `extends` are all such a class.
A trait defers the question to whatever composes it, a closure to whenever it is
bound, and a class that does extend one answers it; those come first, and a
compile fatal runs nothing, so what they prove is that they COMPILED.
--FILE--
<?php
echo "unreachable\n";
trait TskT { public function t() { return parent::t(); } }
class TskBase { public function f() {} }
class TskKid extends TskBase { public function f() { return parent::f(); } }
class TskA { public function m() { return fn () => parent::X; } }
enum TskE {
    case One;
    public function f() { return parent::f(); }
}
?>
--EXPECTF--
%ACannot use "parent" when current class scope has no parent in %s on line 9%A
