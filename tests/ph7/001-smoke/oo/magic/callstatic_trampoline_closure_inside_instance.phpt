--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unbound __callStatic closure keeps its catch-all when invoked inside an instance of its class
--DESCRIPTION--
A closure built with no receiver over a name no method answers is php's __callStatic
trampoline, decided where it was built and kept: invoked later from inside an instance of
the class, which also declares __call, it still runs __callStatic with static:: the class it
was taken through. It used to re-pick the catch-all against the caller's `$this`: the
direct `$c()` died "Non-static method ... cannot be called statically", and every callback
door ran __call on the caller's object instead. A private method the closure's creator
could not reach is a trampoline too, even where the caller could.
--FILE--
<?php
class TrmA {
    private static function p() { return "TrmA::p"; }
    public function __call($n, $a) { return "call:$n:" . static::class . ":" . count($a); }
    public static function __callStatic($n, $a) { return "static:$n:" . static::class . ":" . count($a); }
    static function mk() { return Closure::fromCallable('TrmA::zz'); }
    function viaPriv($c) { return $c(); }
}
class TrmB extends TrmA {
    function t($c, $tag) {
        foreach ([
            'direct' => fn() => $c(1, 2),
            'cuf' => fn() => call_user_func($c, 1),
            'cufa' => fn() => call_user_func_array($c, [1, 2, 3]),
            'map' => fn() => implode(',', array_map($c, [1])),
            'invoke' => fn() => $c->__invoke(),
        ] as $k => $f) {
            try {
                echo "$tag $k: ", $f(), "\n";
            } catch (Throwable $e) {
                echo "$tag $k: ", get_class($e), ": ", $e->getMessage(), "\n";
            }
        }
    }
}
$b = new TrmB;
$b->t(Closure::fromCallable('TrmA::zz'), 'str');
$b->t(Closure::fromCallable(['TrmA', 'zz']), 'pair');
$b->t(TrmA::zz(...), 'fcc');
$b->t(TrmB::zz(...), 'fccB');
$b->t(Closure::fromCallable('TrmA::p'), 'priv');
$b->t(TrmA::mk(), 'mk');
$b->t(Closure::bind(TrmA::zz(...), null, TrmA::class), 'bind');
echo $b->viaPriv(Closure::fromCallable('TrmA::p')), "\n";
$r = new ReflectionFunction(TrmA::zz(...));
var_dump($r->isStatic(), $r->getClosureThis());
--EXPECT--
str direct: static:zz:TrmA:2
str cuf: static:zz:TrmA:1
str cufa: static:zz:TrmA:3
str map: static:zz:TrmA:1
str invoke: static:zz:TrmA:0
pair direct: static:zz:TrmA:2
pair cuf: static:zz:TrmA:1
pair cufa: static:zz:TrmA:3
pair map: static:zz:TrmA:1
pair invoke: static:zz:TrmA:0
fcc direct: static:zz:TrmA:2
fcc cuf: static:zz:TrmA:1
fcc cufa: static:zz:TrmA:3
fcc map: static:zz:TrmA:1
fcc invoke: static:zz:TrmA:0
fccB direct: static:zz:TrmB:2
fccB cuf: static:zz:TrmB:1
fccB cufa: static:zz:TrmB:3
fccB map: static:zz:TrmB:1
fccB invoke: static:zz:TrmB:0
priv direct: static:p:TrmA:2
priv cuf: static:p:TrmA:1
priv cufa: static:p:TrmA:3
priv map: static:p:TrmA:1
priv invoke: static:p:TrmA:0
mk direct: static:zz:TrmA:2
mk cuf: static:zz:TrmA:1
mk cufa: static:zz:TrmA:3
mk map: static:zz:TrmA:1
mk invoke: static:zz:TrmA:0
bind direct: static:zz:TrmA:2
bind cuf: static:zz:TrmA:1
bind cufa: static:zz:TrmA:3
bind map: static:zz:TrmA:1
bind invoke: static:zz:TrmA:0
static:p:TrmA:0
bool(true)
NULL
