--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static `self::`/`parent::` method closure forwards the caller's called class
--DESCRIPTION--
`self::sf(...)` and `parent::sf(...)` naming a static method are forwarding
calls in php: the closure runs the method the keyword resolved to, but through
the caller's late-static-binding class, so `static::` inside it -- and
getClosureCalledClass() -- answer that class, directly, as a callback and after
the closure has left the frame. The scope stays the declaring class. An explicit
class name does not forward, and a rebound scope replaces the forwarded class.
--FILE--
<?php
class FsmcA {
    public static function sf($x = '') { return static::class . $x; }
    public static function inst() { return static::class; }
}
class FsmcB extends FsmcA {
    public static function sf($x = '') {
        $c = parent::sf(...);
        $r = new ReflectionFunction($c);
        return [$c(1), call_user_func($c, 2), array_map($c, [3])[0],
            $r->getClosureCalledClass()->name, $r->getClosureScopeClass()->name];
    }
    public static function kw() { $c = PARENT::sf(...); $d = Self::inst(...); return [$c(5), $d()]; }
    public static function named() { $c = static::inst(...); $d = FsmcB::inst(...); $e = FsmcA::inst(...); return [$c(), $d(), $e()]; }
    public static function rebound() { return Closure::bind(parent::sf(...), null, FsmcA::class)(6); }
    public static function escaped() { return parent::sf(...); }
    public function viaThis() { $c = parent::sf(...); $d = self::inst(...); return [$c(8), $d()]; }
}
class FsmcC extends FsmcB {}
echo json_encode(FsmcC::sf()), "\n";
echo json_encode(FsmcB::sf()), "\n";
echo json_encode(FsmcC::kw()), "\n";
echo json_encode(FsmcC::named()), "\n";
echo FsmcC::rebound(), "\n";
$fsmcF = FsmcC::escaped();
echo $fsmcF(7), "\n";
echo json_encode((new FsmcC)->viaThis()), "\n";
?>
--EXPECT--
["FsmcC1","FsmcC2","FsmcC3","FsmcC","FsmcA"]
["FsmcB1","FsmcB2","FsmcB3","FsmcB","FsmcA"]
["FsmcC5","FsmcC"]
["FsmcC","FsmcB","FsmcA"]
FsmcA6
FsmcC7
["FsmcC8","FsmcC"]
