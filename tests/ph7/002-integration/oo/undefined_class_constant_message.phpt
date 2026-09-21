--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Undefined Class::CONST via :: is a catchable "Undefined constant" Error
--DESCRIPTION--
A bareword `Class::MISSING` (and `self::`/`parent::`, enum cases) names a CONSTANT, so an
undefined one is a catchable `Error: Undefined constant Class::MISSING` — distinct from the
`$`-form `Class::$missing`, which is `Access to undeclared static property`. The two forms
share one lookup miss in the engine; they are told apart by the same
constant-vs-static-property signal that lets `const C` and `public $C` coexist.
--FILE--
<?php
function tc(callable $c) { try { $c(); echo "NO-THROW\n"; } catch (\Error $e) { echo $e->getMessage(), "\n"; } }

class C { const K = 1; public static $s = 2; }
class B { const A = 1; }
class D extends B { public static function f() { return self::MISS; } }
enum E { case A; }

tc(fn() => C::MISSING);          // undefined constant (bareword)
tc(fn() => C::$missing);         // undefined static property ($-form)
tc(function () { $p = "missing"; return C::$$p; });  // dynamic $-form static property
tc(fn() => D::f());              // self::MISS -> undefined constant, named on the LSB class
tc(fn() => E::NOPE);             // undefined enum case is an undefined constant
echo C::K, "/", C::$s, "\n";     // the real members still resolve
?>
--EXPECT--
Undefined constant C::MISSING
Access to undeclared static property C::$missing
Access to undeclared static property C::$missing
Undefined constant D::MISS
Undefined constant E::NOPE
1/2
