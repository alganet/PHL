--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure expression made inside a method reports its scope, called class and $this to Reflection
--DESCRIPTION--
The engine keeps a closure expression's binding on its per-instantiation function and names it
on the Closure object only once bind()/bindTo() clones it, so Reflection -- reading the object --
answered NULL for all three getters while the body itself saw `self`, `static` and `$this`.
A trait method's closure is scoped to the using class; a rebind stays the whole answer.
--FILE--
<?php
trait RceT { function tm() { return function () {}; } static function ts() { return fn () => 1; } }
class RceC {
    use RceT;
    function m() { return function () { return [self::class, static::class, get_class($this)]; }; }
    function a() { return fn () => $this; }
    function st() { return static function () {}; }
    static function s() { return function () {}; }
    function nest() { return function () { return function () {}; }; }
}
class RceE extends RceC {}
function rceShow($label, Closure $c) {
    $r = new ReflectionFunction($c);
    $t = $r->getClosureThis();
    echo $label, ': ', $r->getClosureScopeClass()?->name ?? '-', ' / ',
        $r->getClosureCalledClass()?->name ?? '-', ' / ', $t ? get_class($t) : '-', "\n";
}
$e = new RceE;
rceShow('method', $e->m());
echo implode(',', $e->m()()), "\n";
rceShow('arrow', $e->a());
rceShow('static in method', $e->st());
rceShow('static method', RceE::s());
rceShow('nested', $e->nest()());
rceShow('trait', $e->tm());
rceShow('trait static', RceE::ts());
rceShow('global', function () {});
rceShow('rebound', $e->m()->bindTo(new RceC));
rceShow('scope only', Closure::bind(function () {}, null, 'RceC'));
var_dump((new ReflectionFunction($e->m()))->getClosureThis() === $e);
--EXPECT--
method: RceC / RceE / RceE
RceC,RceE,RceE
arrow: RceC / RceE / RceE
static in method: RceC / RceE / -
static method: RceC / RceE / -
nested: RceC / RceE / RceE
trait: RceC / RceE / RceE
trait static: RceC / RceE / -
global: - / - / -
rebound: RceC / RceC / RceC
scope only: RceC / RceC / -
bool(true)
