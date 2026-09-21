--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__TRAIT__ magic constant: declaring trait name, lexical resolution
--FILE--
<?php
trait T {
    public function who() { return __TRAIT__; }
    public static function swho() { return __TRAIT__; }
    public function viaClosure() { $f = function(){ return __TRAIT__; }; return $f(); }
    public function viaArrow() { $f = fn() => __TRAIT__; return $f(); }
    public function viaStaticClosure() { $f = static function(){ return __TRAIT__; }; return $f(); }
    public function viaNested() { $f = function(){ $g = fn() => __TRAIT__; return $g(); }; return $f(); }
    public function viaAnon() {
        // An anonymous class is a fresh scope: __TRAIT__ is "" inside it,
        // NOT the enclosing trait.
        return (new class { public function inner(){ return __TRAIT__; } })->inner();
    }
    public function viaAnonArrow() {
        // An arrow-fn inside an anonymous class's method: still the anon-class scope -> "".
        return (new class { public function z(){ $a = fn() => __TRAIT__; return $a(); } })->z();
    }
    public function viaMatch($n) {
        // A match() arm/condition runs inside a SYNTHETIC function block that carries no
        // ph7_vm_func; __TRAIT__ must stay lexical and still resolve to the trait.
        return match($n) { 1 => __TRAIT__, default => 'other' };
    }
    public function inMethod() { return __TRAIT__; }
}
trait U {
    public function f() { return __TRAIT__; }
}

// A trait that itself uses another trait: the DECLARING trait wins.
trait A { public function x(){ return __TRAIT__; } }
trait B { use A; }

class C { use T; }
class D { use T; use U; }   // adopted into several classes: still "T"/"U"
class E { use B; }
class Plain { public function m(){ return __TRAIT__; } } // non-trait method -> ""

$c = new C;
var_dump($c->who());        // "T"
var_dump(C::swho());        // "T"
var_dump($c->viaClosure()); // "T" (closures are transparent)
var_dump($c->viaArrow());   // "T"
var_dump($c->viaStaticClosure()); // "T" (static closure, no captures, still transparent)
var_dump($c->viaNested());  // "T" (nested closure + arrow)
var_dump($c->viaAnon());    // "" (anon class is a new scope)
var_dump($c->viaAnonArrow()); // "" (arrow inside anon-class method inside trait)
var_dump($c->viaMatch(1));  // "T" (match arm, synthetic block)
var_dump($c->inMethod());   // "T"

$d = new D;
var_dump($d->who());        // "T"
var_dump($d->f());          // "U"

var_dump((new E)->x());     // "A" (declaring trait, not B or E)

var_dump((new Plain)->m()); // "" (non-trait class method)

function g(){ return __TRAIT__; }
var_dump(g());              // "" (plain function)
var_dump(__TRAIT__);        // "" (global scope)
?>
--EXPECT--
string(1) "T"
string(1) "T"
string(1) "T"
string(1) "T"
string(1) "T"
string(1) "T"
string(0) ""
string(0) ""
string(1) "T"
string(1) "T"
string(1) "T"
string(1) "U"
string(1) "A"
string(0) ""
string(0) ""
string(0) ""
--CLEAN--
<?php
