--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Closure::fromCallable() trampoline over __call/__callStatic refuses a named argument
--DESCRIPTION--
php builds the closure Closure::fromCallable() returns for a name only a catch-all answers as
an internal function with no parameters, so a named argument is "Unknown named parameter"
before the catch-all runs -- at every door that passes one: the direct call, a string-keyed
unpack, call_user_func() and call_user_func_array(). The `(...)` syntax builds its trampoline
with a variadic `...$arguments` instead, and packs the name into $args. Both used to pack it.
--FILE--
<?php
class FcnA {
    public function __call($n, $a) { echo "__call $n ", json_encode($a), "\n"; return 'c'; }
    public static function __callStatic($n, $a) { echo "__callStatic $n ", json_encode($a), "\n"; return 's'; }
}
function fcnTry($label, $f) {
    try {
        $r = $f();
        echo $label, ": ", var_export($r, true), "\n";
    } catch (Error $e) {
        echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
$o = new FcnA;
foreach ([
    'object' => Closure::fromCallable([$o, 'zz']),
    'class' => Closure::fromCallable(['FcnA', 'zz']),
    'string' => Closure::fromCallable('FcnA::zz'),
    'rebound' => Closure::bind(Closure::fromCallable([$o, 'zz']), new FcnA, FcnA::class),
] as $k => $c) {
    echo "== $k\n";
    fcnTry('named', fn() => $c(1, x: 2));
    fcnTry('named-only', fn() => $c(x: 2));
    fcnTry('unpack', fn() => $c(...['x' => 2]));
    fcnTry('cuf', fn() => call_user_func($c, 1, x: 2));
    fcnTry('cufa', fn() => call_user_func_array($c, [1, 'x' => 2]));
    fcnTry('positional', fn() => $c(1, 2));
    fcnTry('unpack-positional', fn() => $c(...[1, 2]));
    fcnTry('cufa-positional', fn() => call_user_func_array($c, [1, 2]));
    fcnTry('map', fn() => array_map($c, [1]));
}
echo "== syntax\n";
$s = $o->zz(...);
fcnTry('object', fn() => $s(1, x: 2));
$s = FcnA::zz(...);
fcnTry('class', fn() => $s(1, x: 2));
fcnTry('cufa', fn() => call_user_func_array($s, ['x' => 2]));
echo "== finally\n";
function fcnFin($c) {
    try {
        return $c(x: 1);
    } finally {
        echo "finally ran\n";
    }
}
fcnTry('fin', fn() => fcnFin(Closure::fromCallable('FcnA::zz')));
echo "done\n";
--EXPECT--
== object
named: Error: Unknown named parameter $x
named-only: Error: Unknown named parameter $x
unpack: Error: Unknown named parameter $x
cuf: Error: Unknown named parameter $x
cufa: Error: Unknown named parameter $x
__call zz [1,2]
positional: 'c'
__call zz [1,2]
unpack-positional: 'c'
__call zz [1,2]
cufa-positional: 'c'
__call zz [1]
map: array (
  0 => 'c',
)
== class
named: Error: Unknown named parameter $x
named-only: Error: Unknown named parameter $x
unpack: Error: Unknown named parameter $x
cuf: Error: Unknown named parameter $x
cufa: Error: Unknown named parameter $x
__callStatic zz [1,2]
positional: 's'
__callStatic zz [1,2]
unpack-positional: 's'
__callStatic zz [1,2]
cufa-positional: 's'
__callStatic zz [1]
map: array (
  0 => 's',
)
== string
named: Error: Unknown named parameter $x
named-only: Error: Unknown named parameter $x
unpack: Error: Unknown named parameter $x
cuf: Error: Unknown named parameter $x
cufa: Error: Unknown named parameter $x
__callStatic zz [1,2]
positional: 's'
__callStatic zz [1,2]
unpack-positional: 's'
__callStatic zz [1,2]
cufa-positional: 's'
__callStatic zz [1]
map: array (
  0 => 's',
)
== rebound
named: Error: Unknown named parameter $x
named-only: Error: Unknown named parameter $x
unpack: Error: Unknown named parameter $x
cuf: Error: Unknown named parameter $x
cufa: Error: Unknown named parameter $x
__call zz [1,2]
positional: 'c'
__call zz [1,2]
unpack-positional: 'c'
__call zz [1,2]
cufa-positional: 'c'
__call zz [1]
map: array (
  0 => 'c',
)
== syntax
__call zz {"0":1,"x":2}
object: 'c'
__callStatic zz {"0":1,"x":2}
class: 's'
__callStatic zz {"x":2}
cufa: 's'
== finally
finally ran
fin: Error: Unknown named parameter $x
done
