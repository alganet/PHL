--TEST--
A fiber body's argument errors: no call site, the method's class, the variadic's own numbering
--FILE--
<?php
// Fiber::start() is an internal function, so php names no `called in` site for a
// body's parameter; a method body is qualified by its declaring class and resolves
// `self` against it; a variadic element is numbered by its place in the call and
// named by no parameter. A generator's arguments are bound at a userland call, so
// they keep the call site.
class K { function m(self $x) { echo "m ok\n"; } static function s(int ...$x) {} }
class L extends K {}
trait T { function t(self $x) { echo "t ok\n"; } }
class U { use T; }
function f(int $x) {}
function h(K ...$k) {}
function g(int $a, int ...$x) { yield 1; }
function show(callable $c) {
    try { $c(); } catch (TypeError $e) {
        echo str_replace(__FILE__, 'FILE', $e->getMessage()), "\n";
        var_dump(isset($e->getTrace()[0]['file']));
    }
}
show(fn() => (new Fiber('f'))->start("a"));
show(fn() => (new Fiber([new L, 'm']))->start(new stdClass));
(new Fiber([new L, 'm']))->start(new K);
show(fn() => (new Fiber('L::s'))->start(1, 2, "x"));
show(fn() => (new Fiber('L::s'))->start(1, a: 2, b: "x"));
show(fn() => (new Fiber('h'))->start(new K, 3));
show(fn() => (new Fiber([new U, 't']))->start(new K));
(new Fiber([new U, 't']))->start(new U);
show(fn() => g(1, 2, "x"));
show(fn() => g(1, 2, q: "x"));
--EXPECT--
f(): Argument #1 ($x) must be of type int, string given
bool(false)
K::m(): Argument #1 ($x) must be of type K, stdClass given
bool(false)
m ok
K::s(): Argument #3 must be of type int, string given
bool(false)
K::s(): Argument #2 must be of type int, string given
bool(false)
h(): Argument #2 must be of type K, int given
bool(false)
U::t(): Argument #1 ($x) must be of type U, K given
bool(false)
t ok
g(): Argument #3 must be of type int, string given, called in FILE on line 28
bool(true)
g(): Argument #3 must be of type int, string given, called in FILE on line 29
bool(true)
