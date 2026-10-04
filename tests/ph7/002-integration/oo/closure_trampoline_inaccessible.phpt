--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable over an inaccessible method is the __callStatic trampoline
--FILE--
<?php
// A first-class callable over a method the creating scope cannot reach is php's call
// trampoline whenever the class declares the catch-all: the static spelling reaches
// __callStatic, and only a class with neither catch-all refuses the visibility.
class A {
    private static function priv(int $x) { return "priv"; }
    protected static function prot() { return "prot"; }
    private function ipriv() { return "ipriv"; }
    public static function __callStatic($n, $a) { return "cs:$n:" . implode(",", $a); }
}
class B extends A {}
class C { private static function p() {} }
class D { private static function p() {} public function __call($n, $a) { return "c:$n"; } }
class E { function t() { return A::priv(...); } }

foreach (['A::priv', 'A::prot', 'A::ipriv', 'B::priv', 'C::p', 'D::p'] as $s) {
    try {
        $f = match ($s) {
            'A::priv' => A::priv(...), 'A::prot' => A::prot(...), 'A::ipriv' => A::ipriv(...),
            'B::priv' => B::priv(...), 'C::p' => C::p(...), 'D::p' => D::p(...),
        };
        $r = new ReflectionFunction($f);
        echo $s, ": ", $f(1, 2), " | ", $r->getName(), " scope=", $r->getClosureScopeClass()->getName(),
            " static=", var_export($r->isStatic(), true), "\n";
    } catch (Error $e) {
        echo $s, ": ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

$f = (new E)->t();
echo $f(7), "\n";
var_dump($f);
$r = new ReflectionFunction($f);
var_dump($r->getNumberOfParameters(), $r->isVariadic(), $r->isInternal(), $r->getParameters()[0]->getName());
echo $r;
--EXPECTF--
A::priv: cs:priv:1,2 | priv scope=A static=true
A::prot: cs:prot:1,2 | prot scope=A static=true
A::ipriv: cs:ipriv:1,2 | ipriv scope=A static=true
B::priv: cs:priv:1,2 | priv scope=A static=true
C::p: Error: Call to private method C::p() from global scope
D::p: Error: Call to private method D::p() from global scope
cs:priv:7
object(Closure)#%d (2) {
  ["function"]=>
  string(7) "A::priv"
  ["parameter"]=>
  array(1) {
    ["$arguments"]=>
    string(10) "<optional>"
  }
}
int(1)
bool(true)
bool(true)
string(9) "arguments"
Closure [ <internal> static public method priv ] {

  - Parameters [1] {
    Parameter #0 [ <optional> mixed ...$arguments ]
  }
}
