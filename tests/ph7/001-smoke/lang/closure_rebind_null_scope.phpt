--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure rebound to a null scope runs in no class, or in Closure's when it has a receiver
--FILE--
<?php
/* bindTo()'s scope argument defaults to "static", which KEEPS the scope, while an explicit
 * null drops it: with a receiver the closure runs in php's dummy `Closure` scope, without
 * one in no class at all. A rebind keeps whatever scope the closure has by then -- the one
 * it was written in, the one a rebind named, or none -- and `static::` is the receiver's
 * class whenever there is one. */
class RebindNullC {
    private static function ps() { return 'ps'; }
    function f() {
        return function () {
            return [self::class, static::class, (new Exception)->getTrace()[0]['class'] ?? '-',
                isset($this) ? get_class($this) : '-'];
        };
    }
    static function g() {
        return static function () {
            try { $r = RebindNullC::ps(); } catch (Error $e) { $r = $e->getMessage(); }
            return [(new Exception)->getTrace()[0]['class'] ?? '-', $r];
        };
    }
    static function gs() {
        return static function () { return [self::class, static::class]; };
    }
}
class RebindNullD {}
trait RebindNullT { function tf() { return function () { return [self::class, static::class]; }; } }
class RebindNullU { use RebindNullT; }
function rebind_null_run($label, $c) {
    $r = new ReflectionFunction($c);
    echo $label, ': ', json_encode([$r->getClosureScopeClass()?->name,
        $r->getClosureCalledClass()?->name,
        $r->getClosureThis() ? get_class($r->getClosureThis()) : null]), ' ', json_encode($c()), "\n";
}
$c = (new RebindNullC)->f();
$d = new RebindNullD;
rebind_null_run('written', $c);
rebind_null_run('bindTo($d)', $c->bindTo($d));
rebind_null_run('bindTo($d, null)', $c->bindTo($d, null));
rebind_null_run('bindTo($d, D)', $c->bindTo($d, RebindNullD::class));
rebind_null_run('bindTo($d, null)->bindTo($d)', $c->bindTo($d, null)->bindTo($d));
rebind_null_run('bindTo($d, null)->bindTo($d, C)', $c->bindTo($d, null)->bindTo($d, 'RebindNullC'));
$s = RebindNullC::g();
rebind_null_run('static', $s);
rebind_null_run('static bindTo(null)', $s->bindTo(null));
rebind_null_run('static bind(null)', Closure::bind($s, null));
rebind_null_run('static bindTo(null, null)', $s->bindTo(null, null));
rebind_null_run('static bindTo(null, null)->bindTo(null)', $s->bindTo(null, null)->bindTo(null));
$g = RebindNullC::gs();
rebind_null_run('static bindTo(null, D)', $g->bindTo(null, RebindNullD::class));
rebind_null_run('static bindTo(null, D)->bindTo(null)', $g->bindTo(null, RebindNullD::class)->bindTo(null));
$top = function () { return [isset($this) ? get_class($this) : '-', static::class]; };
rebind_null_run('top bindTo($d)', $top->bindTo($d));
rebind_null_run('top bindTo($d, null)', $top->bindTo($d, null));
rebind_null_run('top bindTo($d, C)', $top->bindTo($d, 'RebindNullC'));
$t = (new RebindNullU)->tf();
rebind_null_run('trait bindTo(new U)', $t->bindTo(new RebindNullU));
rebind_null_run('trait bindTo($d)', $t->bindTo($d));
?>
--EXPECT--
written: ["RebindNullC","RebindNullC","RebindNullC"] ["RebindNullC","RebindNullC","RebindNullC","RebindNullC"]
bindTo($d): ["RebindNullC","RebindNullD","RebindNullD"] ["RebindNullC","RebindNullD","RebindNullC","RebindNullD"]
bindTo($d, null): ["Closure","RebindNullD","RebindNullD"] ["Closure","RebindNullD","Closure","RebindNullD"]
bindTo($d, D): ["RebindNullD","RebindNullD","RebindNullD"] ["RebindNullD","RebindNullD","RebindNullD","RebindNullD"]
bindTo($d, null)->bindTo($d): ["Closure","RebindNullD","RebindNullD"] ["Closure","RebindNullD","Closure","RebindNullD"]
bindTo($d, null)->bindTo($d, C): ["RebindNullC","RebindNullD","RebindNullD"] ["RebindNullC","RebindNullD","RebindNullC","RebindNullD"]
static: ["RebindNullC","RebindNullC",null] ["RebindNullC","ps"]
static bindTo(null): ["RebindNullC","RebindNullC",null] ["RebindNullC","ps"]
static bind(null): ["RebindNullC","RebindNullC",null] ["RebindNullC","ps"]
static bindTo(null, null): [null,null,null] ["-","Call to private method RebindNullC::ps() from global scope"]
static bindTo(null, null)->bindTo(null): [null,null,null] ["-","Call to private method RebindNullC::ps() from global scope"]
static bindTo(null, D): ["RebindNullD","RebindNullD",null] ["RebindNullD","RebindNullD"]
static bindTo(null, D)->bindTo(null): ["RebindNullD","RebindNullD",null] ["RebindNullD","RebindNullD"]
top bindTo($d): ["Closure","RebindNullD","RebindNullD"] ["RebindNullD","RebindNullD"]
top bindTo($d, null): ["Closure","RebindNullD","RebindNullD"] ["RebindNullD","RebindNullD"]
top bindTo($d, C): ["RebindNullC","RebindNullD","RebindNullD"] ["RebindNullD","RebindNullD"]
trait bindTo(new U): ["RebindNullU","RebindNullU","RebindNullU"] ["RebindNullU","RebindNullU"]
trait bindTo($d): ["RebindNullU","RebindNullD","RebindNullD"] ["RebindNullU","RebindNullD"]
