--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The engine's own function-table keys are not functions a script can name
--DESCRIPTION--
PHL keeps its compiled functions in one table that also holds every mounted class
METHOD, keyed `[__Class@meth_xxxxxxxxxx]`, and every closure, keyed `[closure_N]`.
Every surface that asks whether a function exists looked the name up there raw, so
those keys answered TRUE to function_exists() and is_callable(), resolved for
ReflectionFunction and `new Fiber(name)`, and DISPATCHED. Reaching a method that
way ran a body that assumes the receiver frame a plain-function call never builds:
`$n = '[__Foo@bar_...]'; $n();` popped past the bottom of the operand stack — a
SIGSEGV in the release build, an ASan heap-buffer-overflow read under sanitizers.
No php label can hold a `[`, an `@` or a `]`, so php answers "does not exist" to
every one of them. The engine's OWN by-name dispatch of the same entries — an
OP_MEMBER method resolution, a closure unwrap, the two synthetic call builders —
is marked and still resolves, which is what the second half of this test checks.
(`new Fiber($name)` asks the same question and is covered by its own test: its
argument is not screened as a callable at all.)
--FILE--
<?php
$closure = function () { return 'C'; };
$second  = function () { return 'D'; };

foreach (['[closure_0]', '[closure_1]', '[__Foo@bar_abcdefghij]', '[__Error@getMessage_abcdefghij]'] as $n) {
    echo "-- $n\n";
    var_dump(function_exists($n));
    var_dump(is_callable($n));
    try { $n(); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { call_user_func($n); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { array_map($n, [1]); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
    try { new ReflectionFunction($n); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* Neither list ever names one. */
$d = get_defined_functions();
var_dump(array_values(array_filter(array_merge($d['internal'], $d['user']),
    fn($n) => str_contains($n, '[') || str_contains($n, '@'))));

/* Everything the engine dispatches BY those same names still runs. */
class FenHost {
    public $v = 'V';
    public function m($x) { return "m$x"; }
    public static function s($x) { return "s$x"; }
    private function priv() { return 'p'; }
    public function reach() { return $this->priv(); }
    public function __call($n, $a) { return "call:$n"; }
    public static function __callStatic($n, $a) { return "static:$n"; }
}
function fenNamed() { return 'named'; }

$o = new FenHost;
echo $o->m(1), ' ', FenHost::s(2), ' ', $o->reach(), ' ', $o->missing(), ' ', FenHost::missingS(), "\n";
$fcc = $o->m(...); $sfcc = FenHost::s(...);
echo $fcc(3), ' ', $sfcc(4), ' ', $closure(), ' ', $second(), "\n";
echo call_user_func($closure), ' ', call_user_func([$o, 'm'], 5), ' ',
     call_user_func('FenHost::s', 6), ' ', call_user_func('fenNamed'), "\n";
var_dump(array_map($closure, [1]));
$bound = Closure::bind(function () { return $this->v; }, $o, FenHost::class);
echo $bound(), "\n";
$fiber = new Fiber($closure); $fiber->start(); echo $fiber->getReturn(), "\n";
$fiber2 = new Fiber('fenNamed'); $fiber2->start(); echo $fiber2->getReturn(), "\n";
echo (new ReflectionFunction($closure))->getNumberOfParameters(), ' ',
     (new ReflectionFunction('fenNamed'))->getName(), "\n";
try { throw new RuntimeException('boom'); } catch (Throwable $t) { echo $t->getMessage(), "\n"; }
echo "END\n";
?>
--EXPECT--
-- [closure_0]
bool(false)
bool(false)
Error: Call to undefined function [closure_0]()
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "[closure_0]" not found or invalid function name
TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "[closure_0]" not found or invalid function name
ReflectionException: Function [closure_0]() does not exist
-- [closure_1]
bool(false)
bool(false)
Error: Call to undefined function [closure_1]()
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "[closure_1]" not found or invalid function name
TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "[closure_1]" not found or invalid function name
ReflectionException: Function [closure_1]() does not exist
-- [__Foo@bar_abcdefghij]
bool(false)
bool(false)
Error: Call to undefined function [__Foo@bar_abcdefghij]()
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "[__Foo@bar_abcdefghij]" not found or invalid function name
TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "[__Foo@bar_abcdefghij]" not found or invalid function name
ReflectionException: Function [__Foo@bar_abcdefghij]() does not exist
-- [__Error@getMessage_abcdefghij]
bool(false)
bool(false)
Error: Call to undefined function [__Error@getMessage_abcdefghij]()
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, function "[__Error@getMessage_abcdefghij]" not found or invalid function name
TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, function "[__Error@getMessage_abcdefghij]" not found or invalid function name
ReflectionException: Function [__Error@getMessage_abcdefghij]() does not exist
array(0) {
}
m1 s2 p call:missing static:missingS
m3 s4 C D
C m5 s6 named
array(1) {
  [0]=>
  string(1) "C"
}
V
C
named
0 fenNamed
boom
END
