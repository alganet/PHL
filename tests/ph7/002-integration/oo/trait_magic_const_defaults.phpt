--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__TRAIT__ in const-expression defaults (property/parameter) resolves to the enclosing trait
--FILE--
<?php
// A property default and a parameter default are const-expressions that belong to
// the class being compiled, not to any lexically-enclosing method. Inside a trait
// they resolve to the trait; inside a non-trait class (or an anonymous class nested
// in a trait method) they are "".
trait T {
    public $prop = __TRAIT__;                         // "T"
    public function pdef($x = __TRAIT__) { return $x; } // param default -> "T"
    public function getProp() { return $this->prop; }
    public function anonProp() {
        // Anonymous class nested in a trait method: its property default belongs to
        // the anon class (not a trait) -> "".
        $o = new class { public $q = __TRAIT__; public function g(){ return $this->q; } };
        return $o->g();
    }
    public function anonParam() {
        $o = new class { public function h($y = __TRAIT__){ return $y; } };
        return $o->h();
    }
}
class C { use T; }
$c = new C;
var_dump($c->getProp());    // "T"
var_dump($c->pdef());       // "T"
var_dump($c->anonProp());   // ""
var_dump($c->anonParam());  // ""

// Non-trait class: property/param defaults are ""
class D {
    public $prop = __TRAIT__;
    public function pdef($x = __TRAIT__) { return $x; }
    public function getProp() { return $this->prop; }
}
$d = new D;
var_dump($d->getProp());    // ""
var_dump($d->pdef());       // ""

// Interface constant default is "" (not a trait)
interface I { const X = __TRAIT__; }
var_dump(I::X);             // ""
?>
--EXPECT--
string(1) "T"
string(1) "T"
string(0) ""
string(0) ""
string(0) ""
string(0) ""
string(0) ""
--CLEAN--
<?php
