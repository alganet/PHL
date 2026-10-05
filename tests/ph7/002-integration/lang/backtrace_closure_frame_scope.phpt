--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure's backtrace frame and by-reference warning name the class it was written in
--DESCRIPTION--
php qualifies a closure by its scope: `C->{closure:C::m():N}()` for one made in an
instance method (the receiver is the frame's object), `C::` for a static closure or
one made in a static method, the rebind's scope after Closure::bind(), `Closure`
for an object bound to a closure written outside any class, and nothing for a
top-level closure never bound. An argument refusal is raised inside the callee, so
its trace already carries the receiver. The by-reference `value given` warning
names the callee the same way.
--FILE--
<?php
function f($s) { return str_replace(__FILE__, 'F', $s); }
function show($tag, $c, ...$a) {
    $f = $c(...$a)[0];
    echo $tag, ": ", $f['class'] ?? '-', ' ', $f['type'] ?? '-', ' ',
        isset($f['object']) ? get_class($f['object']) : '-', "\n";
}
class P {}
class C extends P {
    function inst() { return function () { return debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT); }; }
    function instStatic() { return static function () { return debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT); }; }
    static function stat() { return function () { return debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT); }; }
    function arrow() { return fn() => debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT); }
    function typed() { return function (int $x) {}; }
    function few() { return function ($a, $b) {}; }
    function byRef() { return function (&$r) { $r = 1; }; }
    static function byRefStatic() { return static function (&$r) { $r = 1; }; }
}
class D extends C {}
trait T { function tm() { return function () { return debug_backtrace(); }; } }
class U { use T; }
$top = function () { return debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT); };

show('instance method', (new C)->inst());
show('inherited', (new D)->inst());
show('static closure', (new C)->instStatic());
show('static method', C::stat());
show('arrow fn', (new C)->arrow());
show('trait method', (new U)->tm());
show('top level', $top);
show('bound with scope', Closure::bind($top, new C, C::class));
show('bound, no scope', Closure::bind($top, new C));
show('scope only', Closure::bind($top, null, C::class));
show('rebound', Closure::bind((new C)->inst(), new D, D::class));
show('bound to a subclass', Closure::bind($top, new D, P::class));

function run($c, ...$a) {
    try {
        $c(...$a);
    } catch (Throwable $e) {
        echo get_class($e), ': ', f(explode("\n", $e->getTraceAsString())[0]), "\n";
    }
}
run((new C)->typed(), 'a');
run((new C)->few(), 1);
try {
    array_map((new C)->typed(), ['a']);
} catch (TypeError $e) {
    echo f(explode("\n", $e->getTraceAsString())[0]), "\n";
}

set_error_handler(function ($n, $s) { echo f($s), "\n"; return true; });
array_map((new C)->byRef(), [1]);
array_map(C::byRefStatic(), [1]);
$h = function (&$r) {};
array_map($h, [1]);
array_map(Closure::bind($h, new C, C::class), [1]);
array_map(Closure::bind($h, new C), [1]);
--EXPECT--
instance method: C -> C
inherited: C -> D
static closure: C :: -
static method: C :: -
arrow fn: C -> C
trait method: U -> U
top level: - - -
bound with scope: C -> C
bound, no scope: Closure -> C
scope only: C :: -
rebound: D -> D
bound to a subclass: P -> D
TypeError: #0 F(39): C->{closure:C::typed():14}()
ArgumentCountError: #0 F(39): C->{closure:C::few():15}()
#0 [internal function]: C->{closure:C::typed():14}()
C::{closure:C::byRef():16}(): Argument #1 ($r) must be passed by reference, value given
C::{closure:C::byRefStatic():17}(): Argument #1 ($r) must be passed by reference, value given
{closure:F:55}(): Argument #1 ($r) must be passed by reference, value given
C::{closure:F:55}(): Argument #1 ($r) must be passed by reference, value given
Closure::{closure:F:55}(): Argument #1 ($r) must be passed by reference, value given
