--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass::getConstructor() finds a constructor supplied via a trait `as __construct` alias
--FILE--
<?php
// `use Ctor { init as __construct; }` makes init the constructor under the name
// __construct while keeping it reachable as init. getConstructor() must return the
// __construct one (was null: the aliased entry had been skipped in reflection).
trait Ctor { public function init($v){ $this->v = $v; } }
class W { public $v; use Ctor { init as __construct; } }

$w = new W(9);
var_dump($w->v);                                     // 9 (alias runs as the ctor)

$r = new ReflectionClass('W');
$c = $r->getConstructor();
var_dump($c === null ? 'NULL' : $c->getName());      // "__construct"
var_dump($c->isConstructor());                       // true
var_dump($c->getNumberOfParameters());               // 1
var_dump($c->getDeclaringClass()->getName());        // "W"

// Both names are reachable (getMethods() order is unspecified in php -> sort).
$names = array_map(fn($m) => $m->getName(), $r->getMethods());
sort($names);
var_dump($names);                                    // ["__construct","init"]

// A class with no constructor at all still returns null.
class NoCtor { public function x(){} }
var_dump((new ReflectionClass('NoCtor'))->getConstructor()); // NULL

// An explicit __construct is unaffected.
class E { public function __construct(){} }
var_dump((new ReflectionClass('E'))->getConstructor()->getName()); // "__construct"
?>
--EXPECT--
int(9)
string(11) "__construct"
bool(true)
int(1)
string(1) "W"
array(2) {
  [0]=>
  string(11) "__construct"
  [1]=>
  string(4) "init"
}
NULL
string(11) "__construct"
--CLEAN--
<?php
