--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure unbound from $this does not run on the receiver it captured where it was made
--FILE--
<?php
/* A closure made in a method captures the receiver; one whose body never names `$this`
 * may be unbound (bindTo(null), Closure::bind($c, null, Scope)) and then runs with no
 * `$this` at all, through every door -- and the closure it was made from keeps its own. */
class UnbindDropsRecv {
    function mk() { return function () { return debug_backtrace(); }; }
    function mkg() { return function () { yield debug_backtrace(); }; }
}
function unbind_drops_show($bt, $tag) {
    echo $tag, ': ', $bt[0]['class'] ?? '-', ' ', $bt[0]['type'] ?? '-', ' ',
        isset($bt[0]['object']) ? 'object' : 'no object', "\n";
}
$o = new UnbindDropsRecv;
$c = $o->mk();
$u = $c->bindTo(null);
unbind_drops_show($c(), 'original');
unbind_drops_show($u(), 'unbound');
unbind_drops_show(call_user_func($u), 'call_user_func');
unbind_drops_show(array_map($u, [1])[0], 'array_map');
unbind_drops_show($u->__invoke(), '__invoke');
unbind_drops_show((new ReflectionFunction($u))->invoke(), 'ReflectionFunction::invoke');
unbind_drops_show(Closure::bind($c, null, UnbindDropsRecv::class)(), 'scope only');
unbind_drops_show($c(), 'original again');
unbind_drops_show($u->bindTo($o)(), 'bound again');
foreach ($o->mkg()->bindTo(null)() as $bt) unbind_drops_show($bt, 'generator');
foreach ($o->mkg()() as $bt) unbind_drops_show($bt, 'generator original');
$f = new Fiber($u);
$f->start();
unbind_drops_show($f->getReturn(), 'fiber body');
var_dump((new ReflectionFunction($u))->getClosureThis());
--EXPECT--
original: UnbindDropsRecv -> object
unbound: UnbindDropsRecv :: no object
call_user_func: UnbindDropsRecv :: no object
array_map: UnbindDropsRecv :: no object
__invoke: UnbindDropsRecv :: no object
ReflectionFunction::invoke: UnbindDropsRecv :: no object
scope only: UnbindDropsRecv :: no object
original again: UnbindDropsRecv -> object
bound again: UnbindDropsRecv -> object
generator: UnbindDropsRecv :: no object
generator original: UnbindDropsRecv -> object
fiber body: UnbindDropsRecv :: no object
NULL
