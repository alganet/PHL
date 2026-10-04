--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A forwarding `parent::missing()` hands __callStatic the caller's called class
--DESCRIPTION--
`self::`, `parent::` and `static::` are forwarding calls in php, and a name the
class answers only through __callStatic forwards like any static method: the
handler the keyword resolved to runs, but `static::` inside it is the caller's
late-static-binding class, not the class that declares the handler. That holds
for a missing name, for a private static one reached from a subclass, and for
the `parent::missing(...)` closure, directly, as a callback and after it has
left the frame. An explicit class name does not forward, and a child that
overrides __callStatic is still not what `parent::` runs.
--FILE--
<?php
class CsfA {
    public static function __callStatic($n, $a) {
        echo "CsfA $n ", static::class, " ", json_encode($a), "\n";
    }
    private static function priv() { echo "unreachable\n"; }
}
class CsfB extends CsfA {
    public static function s() {
        parent::missing(1);
        self::missing(2);
        static::missing(3);
        CsfA::missing(4);
        parent::priv(5);
        $f = parent::missing(...);
        $f(6);
        array_map($f, [7]);
        $r = new ReflectionFunction($f);
        echo $r->getClosureCalledClass()->name, " ", $r->getClosureScopeClass()->name, "\n";
        return $f;
    }
    public function i() {
        parent::missing(8);
        $f = parent::missing(...);
        $f(9);
    }
}
class CsfC extends CsfB {}
class CsfD extends CsfB {
    public static function __callStatic($n, $a) { echo "CsfD $n ", static::class, "\n"; }
}
$f = CsfC::s();
$f(10);
echo "==\n";
CsfD::s();
echo "==\n";
CsfB::s();
echo "==\n";
(new CsfC)->i();
?>
--EXPECT--
CsfA missing CsfC [1]
CsfA missing CsfC [2]
CsfA missing CsfC [3]
CsfA missing CsfA [4]
CsfA priv CsfC [5]
CsfA missing CsfC [6]
CsfA missing CsfC [7]
CsfC CsfA
CsfA missing CsfC [10]
==
CsfA missing CsfD [1]
CsfA missing CsfD [2]
CsfD missing CsfD
CsfA missing CsfA [4]
CsfA priv CsfD [5]
CsfA missing CsfD [6]
CsfA missing CsfD [7]
CsfD CsfA
==
CsfA missing CsfB [1]
CsfA missing CsfB [2]
CsfA missing CsfB [3]
CsfA missing CsfA [4]
CsfA priv CsfB [5]
CsfA missing CsfB [6]
CsfA missing CsfB [7]
CsfB CsfA
==
CsfA missing CsfC [8]
CsfA missing CsfC [9]
