--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to a method reached through a callable value is refused where it is sent
--FILE--
<?php
/* php resolves a callable value's target before its arguments run, and a NAME at
 * the send of its own argument against that target: an array pair, a "Class::m"
 * string, an __invoke object and a Closure built from a method all throw
 * `Unknown named parameter` (or `overwrites previous argument`) before a later
 * argument runs and before a plain variable operand is read. A name only
 * __call/__callStatic answers is theirs to receive. */
namespace Ncs;
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function s() { echo "  s ran\n"; return 1; }
class P { function pm($a) { return "P"; } }
class C extends P {
    function m($a) { return $a; }
    static function sm($a) { return $a; }
    function __invoke($a) { return $a; }
    private function priv($a) { return $a; }
    function v($a, ...$rest) { return $rest; }
    function pm($b) { return "C"; }
    function viaParent() { return parent::pm(...); }
    function privPair() { return [$this, 'priv']; }
    function callPriv() { [$this, 'priv'](zz: $u, a: s()); }
}
class M {
    function __call($n, $a) { echo "  __call $n ", json_encode($a), "\n"; }
    static function __callStatic($n, $a) { echo "  __callStatic $n ", json_encode($a), "\n"; }
}
$o = new C;
$cases = [
    'bound closure' => function () use ($o) { $c = (function ($a) {})->bindTo($o); $c(zz: $u, a: s()); },
    'scoped closure' => function () { $c = \Closure::bind(function ($a) {}, null, C::class); $c(zz: $u, a: s()); },
    'method closure' => function () use ($o) { $c = $o->m(...); $c(zz: $u, a: s()); },
    'static method closure' => function () { $c = C::sm(...); $c(zz: $u, a: s()); },
    'parent:: closure' => function () use ($o) { $c = $o->viaParent(); $c(b: $u, a: s()); },
    'parent:: closure runs' => function () use ($o) { $c = $o->viaParent(); var_dump($c(a: 1)); },
    'fromCallable pair' => function () use ($o) { $c = \Closure::fromCallable([$o, 'm']); $c(zz: $u, a: s()); },
    'fromCallable object' => function () use ($o) { $c = \Closure::fromCallable($o); $c(zz: $u, a: s()); },
    '__invoke object' => function () use ($o) { $o(zz: $u, a: s()); },
    'object pair' => function () use ($o) { [$o, 'm'](zz: $u, a: s()); },
    'class pair' => function () { ['Ncs\C', 'sm'](zz: $u, a: s()); },
    'Class::m string' => function () { 'Ncs\C::sm'(zz: $u, a: s()); },
    '\\Class::m string' => function () { '\Ncs\C::sm'(zz: $u, a: s()); },
    'pair in a variable' => function () use ($o) { $p = [$o, 'm']; $p(zz: $u, a: s()); },
    'overwrite through __invoke' => function () use ($o) { $o(1, a: $u); },
    'overwrite through a pair' => function () use ($o) { [$o, 'm'](1, a: $u); },
    'variadic collects' => function () use ($o) { var_dump([$o, 'v'](a: 1, zz: s())); },
    'private, from outside' => function () use ($o) { [$o, 'priv'](zz: $u, a: s()); },
    'private, from inside' => function () use ($o) { $o->callPriv(); },
    'private pair escaped' => function () use ($o) { $p = $o->privPair(); $p(zz: $u, a: s()); },
    '__call pair' => function () { [new M, 'nope'](zz: s(), a: 2); },
    '__callStatic string' => function () { 'Ncs\M::nope'(zz: s(), a: 2); },
    '__call closure' => function () { $c = (new M)->nope(...); $c(zz: s(), a: 2); },
    'native pair' => function () { $d = new \ArrayObject([]); [$d, 'offsetExists'](zz: $u, key: s()); },
    'native string' => function () { '\DateTime::createFromFormat'(zz: $u, format: s()); },
    'native method closure' => function () { $d = new \ArrayObject([]); $c = $d->offsetExists(...); $c(zz: $u, key: s()); },
    'names that bind' => function () use ($o) { var_dump([$o, 'm'](a: 5), 'Ncs\C::sm'(a: 6), $o(a: 7)); },
];
foreach ($cases as $label => $c) {
    echo "$label\n";
    try {
        $c();
    } catch (\Error $e) {
        echo "  ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
--EXPECT--
bound closure
  Error: Unknown named parameter $zz
scoped closure
  Error: Unknown named parameter $zz
method closure
  Error: Unknown named parameter $zz
static method closure
  Error: Unknown named parameter $zz
parent:: closure
  Error: Unknown named parameter $b
parent:: closure runs
string(1) "P"
fromCallable pair
  Error: Unknown named parameter $zz
fromCallable object
  Error: Unknown named parameter $zz
__invoke object
  Error: Unknown named parameter $zz
object pair
  Error: Unknown named parameter $zz
class pair
  Error: Unknown named parameter $zz
Class::m string
  Error: Unknown named parameter $zz
\Class::m string
  Error: Unknown named parameter $zz
pair in a variable
  Error: Unknown named parameter $zz
overwrite through __invoke
  Error: Named parameter $a overwrites previous argument
overwrite through a pair
  Error: Named parameter $a overwrites previous argument
variadic collects
  s ran
array(1) {
  ["zz"]=>
  int(1)
}
private, from outside
  Error: Call to private method Ncs\C::priv() from global scope
private, from inside
  Error: Unknown named parameter $zz
private pair escaped
  Error: Call to private method Ncs\C::priv() from global scope
__call pair
  s ran
  __call nope {"zz":1,"a":2}
__callStatic string
  s ran
  __callStatic nope {"zz":1,"a":2}
__call closure
  s ran
  __call nope {"zz":1,"a":2}
native pair
  Error: Unknown named parameter $zz
native string
  Error: Unknown named parameter $zz
native method closure
  Error: Unknown named parameter $zz
names that bind
int(5)
int(6)
int(7)
