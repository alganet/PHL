--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared hooked property keeps every parent hook it does not write itself
--DESCRIPTION--
A subclass that redeclares a hooked property with only one hook -- or none --
inherits the parent's other hooks, so a child writing only `set` still reads
through the parent's `get`. An abstract parent hook is the exception: a backed
property already performs the operation and inherits nothing. A redeclaration
over a backed parent property is backed whatever its own hook bodies touch,
which also lets it carry a default; over a virtual one it stays virtual, and a
`parent::$x::get()` call does not make it backed.
--FILE--
<?php
class HkInhP { public int $x = 1 { get => $this->x * 10; set => $value + 1; } }
class HkInhC extends HkInhP { public int $x = 2 { set => $value + 100; } }
class HkInhE extends HkInhP { public int $x = 4; }
$c = new HkInhC; var_dump($c->x); $c->x = 5; var_dump($c->x);
$e = new HkInhE; var_dump($e->x); $e->x = 5; var_dump($e->x);
$r = new ReflectionProperty('HkInhC', 'x');
var_dump(array_keys($r->getHooks()));
var_dump($r->getHook(PropertyHookType::Get)->class, $r->getHook(PropertyHookType::Set)->class);
var_dump(array_keys((new ReflectionProperty('HkInhE', 'x'))->getHooks()));

class HkInhV { public int $y { get => 42; } }
class HkInhW extends HkInhV { public int $y { set { echo "set $value\n"; } } }
$w = new HkInhW; var_dump($w->y); $w->y = 3; var_dump($w->y);
var_dump((new ReflectionProperty('HkInhW', 'y'))->isVirtual());
var_dump(get_object_vars($w), (array)$w);

abstract class HkInhA { abstract public int $z { get; } }
class HkInhB extends HkInhA { public int $z = 9 { set => $value * 2; } }
$b = new HkInhB; var_dump($b->z); $b->z = 4; var_dump($b->z);
var_dump(array_keys((new ReflectionProperty('HkInhB', 'z'))->getHooks()));

abstract class HkInhA2 { abstract public int $v { get; set; } }
class HkInhB2 extends HkInhA2 { public int $v { get => 5; set { echo "s\n"; } } }
$b2 = new HkInhB2; var_dump($b2->v); $b2->v = 1;

class HkInhQ { public int $w = 1; }
class HkInhR extends HkInhQ { public int $w = 5 { get => 77; } }
var_dump((new HkInhR)->w, (new ReflectionProperty('HkInhR', 'w'))->isVirtual());

class HkInhG { public $x { get => 1; } }
class HkInhH extends HkInhG { public $x { get => parent::$x::get() + 1; set { echo "H set\n"; } } }
class HkInhI extends HkInhH { public $x { set { echo "I set\n"; } } }
$i = new HkInhI; var_dump($i->x); $i->x = 3;
var_dump((new ReflectionProperty('HkInhH', 'x'))->isVirtual(), (new ReflectionProperty('HkInhI', 'x'))->isVirtual());
var_dump((new ReflectionProperty('HkInhI', 'x'))->getHook(PropertyHookType::Get)->class);
--EXPECT--
int(20)
int(1050)
int(40)
int(60)
array(2) {
  [0]=>
  string(3) "get"
  [1]=>
  string(3) "set"
}
string(6) "HkInhP"
string(6) "HkInhC"
array(2) {
  [0]=>
  string(3) "get"
  [1]=>
  string(3) "set"
}
int(42)
set 3
int(42)
bool(true)
array(1) {
  ["y"]=>
  int(42)
}
array(0) {
}
int(9)
int(8)
array(1) {
  [0]=>
  string(3) "set"
}
int(5)
s
int(77)
bool(false)
int(2)
I set
bool(true)
bool(true)
string(6) "HkInhH"
