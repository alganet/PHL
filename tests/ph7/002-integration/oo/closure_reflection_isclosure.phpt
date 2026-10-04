--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionFunction::isClosure() is true over every Closure object, not only an anonymous one
--FILE--
<?php
// isClosure() answers for every function a Closure object holds -- a
// first-class callable, fromCallable over a function or a method, and a
// __call/__callStatic trampoline -- and for nothing reached by name.
// isAnonymous() stays the narrower {closure} question, and a ReflectionMethod,
// Closure::__invoke included, is never a closure.
function g() {}
class A {
    function m() {}
    static function s() {}
    function __call($n, $a) {}
    static function __callStatic($n, $a) {}
    function mk() { return function () {}; }
}
$cases = [
    'function(){}' => function () {},
    'fn' => fn() => 1,
    'static function' => static function () {},
    "fromCallable('strlen')" => Closure::fromCallable('strlen'),
    'strlen(...)' => strlen(...),
    "fromCallable('g')" => Closure::fromCallable('g'),
    'g(...)' => g(...),
    'fromCallable([$a, m])' => Closure::fromCallable([new A, 'm']),
    'A::s(...)' => A::s(...),
    '__call trampoline' => Closure::fromCallable([new A, 'nope']),
    '__callStatic trampoline' => Closure::fromCallable(['A', 'nope']),
    'bound' => Closure::bind(function () {}, new A, 'A'),
    'from a method' => (new A)->mk(),
    'fromCallable(closure)' => Closure::fromCallable(function () {}),
];
foreach ($cases as $k => $c) {
    $r = new ReflectionFunction($c);
    echo $k, ': ', var_export($r->isClosure(), true), ' ', var_export($r->isAnonymous(), true), "\n";
}
foreach (['strlen', 'g'] as $n) {
    $r = new ReflectionFunction($n);
    echo "by name $n: ", var_export($r->isClosure(), true), ' ', var_export($r->isAnonymous(), true), "\n";
}
foreach ([[function () {}, '__invoke'], [strlen(...), '__invoke'], ['Closure', 'fromCallable'], ['A', 'm']] as [$t, $m]) {
    $r = new ReflectionMethod($t, $m);
    echo 'method ', is_string($t) ? $t : 'Closure', "::$m: ", var_export($r->isClosure(), true), "\n";
}
--EXPECT--
function(){}: true true
fn: true true
static function: true true
fromCallable('strlen'): true false
strlen(...): true false
fromCallable('g'): true false
g(...): true false
fromCallable([$a, m]): true false
A::s(...): true false
__call trampoline: true false
__callStatic trampoline: true false
bound: true true
from a method: true true
fromCallable(closure): true true
by name strlen: false false
by name g: false false
method Closure::__invoke: false
method Closure::__invoke: false
method Closure::fromCallable: false
method A::m: false
