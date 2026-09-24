--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::__construct() screens its callable, and takes every shape php's does
--DESCRIPTION--
`Fiber::__construct()` declares `callable $callback` and then screened it by hand:
"is this a string or an object", with a FiberError of PHL's own wording. So the
three things php answers here were all different — php runs the ordinary
callback-argument screen and raises a TypeError naming exactly why (`new
Fiber('nosuch')` fails at CONSTRUCTION, not one call later at start()), and it
accepts every callable SHAPE, where PHL took a plain function name or a Closure
and nothing else. `[$obj, 'method']`, `['Class', 'stat']` and `"Class::stat"` —
the everyday way to run an object's method as a coroutine — could not be spelled
at all. The two resolvers (the PHP-level `start()` and the C API's) were two
copies of one decision and only one of them ever grew a shape; they are one now.
Three callables php runs and PHL refuses loudly are the `_zend`-twinned §10
divergence in fiber_callable_no_body{,_zend}.phpt.
--FILE--
<?php
class FcaHost {
    public function inst() { return 'inst:' . Fiber::suspend('s'); }
    public static function stat() { return 'stat'; }
    public function __invoke() { return 'invoke'; }
}
$o = new FcaHost;

/* The argument is screened where php screens it, with php's reasons. */
foreach ([['nosuchfn', 'unresolvable name'],
          [42, 'int'],
          [null, 'null'],
          [[1, 2, 3], 'three-element array'],
          [['NoSuchCls', 'm'], 'unknown class'],
          [['FcaHost', 'inst'], 'non-static reached statically']] as [$cb, $label]) {
    try { new Fiber($cb); echo "$label: constructed\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* Every shape php accepts, run to completion. */
$shapes = [
    'closure'      => function () { return 'closure'; },
    'function'     => 'fcaNamed',
    'obj-array'    => [$o, 'inst'],
    'static-array' => ['FcaHost', 'stat'],
    'static-string' => 'FcaHost::stat',
    'static-via-obj' => [$o, 'stat'],
    'invokable'    => $o,
    'fcc'          => $o->stat(...),
];
function fcaNamed() { return 'function'; }
foreach ($shapes as $label => $cb) {
    $f = new Fiber($cb);
    $first = $f->start();
    if (!$f->isTerminated()) { $f->resume('R'); }
    echo $label, ': ', var_export($first, true), ' -> ', var_export($f->getReturn(), true), "\n";
}

/* The receiver really is the object, not a fresh one. */
class FcaCounter {
    public $n = 0;
    public function run() { $this->n++; Fiber::suspend(); $this->n++; return $this->n; }
}
$c = new FcaCounter;
$f = new Fiber([$c, 'run']);
$f->start();
echo 'mid=', $c->n, "\n";
$f->resume();
echo 'end=', $c->n, ' ret=', $f->getReturn(), "\n";
echo "END\n";
?>
--EXPECT--
unresolvable name: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, function "nosuchfn" not found or invalid function name
int: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, no array or string given
null: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, no array or string given
three-element array: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, array callback must have exactly two members
unknown class: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, class "NoSuchCls" not found
non-static reached statically: TypeError: Fiber::__construct(): Argument #1 ($callback) must be a valid callback, non-static method FcaHost::inst() cannot be called statically
closure: NULL -> 'closure'
function: NULL -> 'function'
obj-array: 's' -> 'inst:R'
static-array: NULL -> 'stat'
static-string: NULL -> 'stat'
static-via-obj: NULL -> 'stat'
invokable: NULL -> 'invoke'
fcc: NULL -> 'stat'
mid=1
end=2 ret=2
END
