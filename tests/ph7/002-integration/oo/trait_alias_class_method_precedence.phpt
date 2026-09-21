--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class-declared method overrides a trait `m as name` alias of the same name
--FILE--
<?php
// php: a method declared in the class body wins over a trait alias of the same
// name (the class-declared one is used), while the trait method stays reachable
// under its own name. PHL used to install the alias ahead of the class method
// (LIFO hash insert), so `new` ran the alias instead of the explicit ctor.
trait Ctor { public function init(){ return 'trait-init'; } }

class W {
    public $r = 'none';
    public function __construct(){ $this->r = 'explicit'; }   // must win over the alias
    use Ctor { init as __construct; }
}
$w = new W;
var_dump($w->r);                                   // "explicit" (explicit ctor ran)
var_dump($w->init());                              // "trait-init" (original still reachable)
$ref = new ReflectionClass('W');
var_dump($ref->getConstructor()->getName());       // "__construct"
var_dump($ref->getConstructor()->getDeclaringClass()->getName());  // "W"

// Generalises to any name, not just __construct.
trait Src { public function src(){ return 'trait-src'; } }
class C {
    use Src { src as run; }
    public function run(){ return 'class-run'; }   // class method wins over the alias
}
$c = new C;
var_dump($c->run());    // "class-run"
var_dump($c->src());    // "trait-src"

// An alias to a name the class does NOT declare installs normally.
class D { use Src { src as aliased; } }
var_dump((new D)->aliased());   // "trait-src"
var_dump((new D)->src());       // "trait-src"

// An abstract method declared with the alias name wins too (the alias does not
// silently implement it); a child then provides the implementation.
abstract class A {
    use Src { src as go; }
    abstract public function go();
}
class Child extends A { public function go(){ return 'child-go'; } }
var_dump((new Child)->go());    // "child-go"
var_dump((new Child)->src());   // "trait-src"
?>
--EXPECT--
string(8) "explicit"
string(10) "trait-init"
string(11) "__construct"
string(1) "W"
string(9) "class-run"
string(9) "trait-src"
string(9) "trait-src"
string(9) "trait-src"
string(8) "child-go"
string(9) "trait-src"
--CLEAN--
<?php
