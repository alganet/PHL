--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP-4-style class-name constructors were removed in 8.0: a method named like the class is a plain method
--FILE--
<?php
// A method named like its class must NOT be treated as the constructor
// (regression: PHL aliased it to __construct, so `new C` invoked it as the
// ctor and a required parameter there fataled with ArgumentCountError).
class C {
    public $made = 'default';
    public function C($x) { $this->made = "method:$x"; return $this->made; }
}
$o = new C;                 // no constructor runs -> object just created
var_dump($o->made);         // "default"
var_dump($o->C(7));         // "method:7" (plain method call)

// getConstructor() is null when only a class-name method exists
$r = new ReflectionClass('C');
var_dump($r->getConstructor());

// A class-name method keeps its DECLARED visibility (not forced public)
class Priv {
    public function go() { return $this->Priv(); }
    private function Priv() { return 'inner'; }
}
$p = new Priv;
var_dump($p->go());         // "inner" (reachable internally)
try {
    $p->Priv();             // external call -> Error
} catch (\Error $e) {
    var_dump($e->getMessage());
}

// An explicit __construct still wins and coexists with a same-named method
class Point {
    public $tag;
    public function __construct($t) { $this->tag = "ctor:$t"; }
    public function Point($x) { return "method:$x"; }
}
$pt = new Point('A');
var_dump($pt->tag);                          // "ctor:A"
var_dump($pt->Point('B'));                   // "method:B"
var_dump((new ReflectionClass('Point'))->getConstructor()->getName()); // "__construct"

// Inheritance: a same-named parent method is a normal inherited method
class Base { public function Base() { return 'base-method'; } }
class Child extends Base {}
var_dump((new Child)->Base());               // "base-method"
?>
--EXPECT--
string(7) "default"
string(8) "method:7"
NULL
string(5) "inner"
string(53) "Call to private method Priv::Priv() from global scope"
string(6) "ctor:A"
string(8) "method:B"
string(11) "__construct"
string(11) "base-method"
--CLEAN--
<?php
