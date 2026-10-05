--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scope keyword in a named function's body is refused before anything runs
--DESCRIPTION--
`self::`, `static::`, `parent::`, `new static` and `instanceof self` name a class
relative to the scope they are written in. A named function has none -- even one
written inside a method -- and php knows that while it compiles, so it refuses
the program up front instead of throwing when the line runs. Everywhere the scope
is NOT known yet the question waits for run time: top-level code (an include can
run inside a method), a closure or arrow function (it can be rebound), a trait
(its `self` is the class using it) and a const-expression default. Those come
first here; a compile fatal runs nothing, so what they prove is that they COMPILED.
--FILE--
<?php
echo "unreachable\n";
$top = fn () => self::X;
$cl = function () { return new static; };
trait TskT { public function t() { return parent::t() + static::$p; } }
function tskDefault($a = self::X) {}
class TskA {
    const C = parent::D;
    public function m() {
        return function () { return $x instanceof parent; };
    }
}
function tskOuter() {
    return 1;
}
class TskB {
    public function m() {
        function tskNested() { return self::class; }
    }
}
?>
--EXPECTF--
%ACannot use "self" when no class scope is active in %s on line 18%A
