--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator and Fiber methods are C bodies, and enforce the arity php declares
--FILE--
<?php
/* Every Generator/Fiber method used to be a one-line prelude body forwarding to a
 * global `__gen_verb($this,...)` / `__fiber_verb($this,...)` thunk. They are C
 * bodies now, so the receiver arrives as $this rather than as argument #0, and
 * each one declares a signature that the OP_CALL choke point enforces.
 *
 * Two things that only a declaration can give: `send()` REQUIRES its argument in
 * php (the prelude wrote `$value = null`, so `$gen->send()` answered the first
 * yielded value instead of raising), and a zero-parameter method rejects extras
 * (the prelude enforced no maximum at all). */
function nmgf(){ $x = yield 1; echo "got:$x\n"; yield 2; return 9; }

$it = nmgf();
var_dump($it instanceof Iterator, $it instanceof Traversable);
var_dump($it->current(), $it->key(), $it->valid());
var_dump($it->send('A'));
$it->next();
var_dump($it->valid(), $it->getReturn());

/* the class is still concrete and still an Iterator, though `implements Iterator`
 * is now attached from C after the methods exist */
foreach (nmgf() as $k => $v) { echo "$k=>$v\n"; }

/* Fiber: start() is variadic and its arguments reach the C body directly */
$f = new Fiber(function ($a, $b) { $x = Fiber::suspend($a + $b); return "done:$x"; });
var_dump($f->start(3, 4));
var_dump($f->isSuspended(), $f->isStarted(), $f->isTerminated());
$f->resume('R');
var_dump($f->getReturn(), $f->isTerminated());

/* arity, both bounds */
$g = nmgf();
foreach ([
    'send 0'      => fn () => $g->send(),
    'throw 0'     => fn () => $g->throw(),
    'current 1'   => fn () => $g->current(1),
    'isStarted 1' => fn () => (new Fiber(function () {}))->isStarted(1),
    'new Fiber 0' => fn () => new Fiber(),
] as $label => $t) {
    try { $t(); echo "$label: no error\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

/* the thunks are gone */
var_dump(function_exists('__gen_next'), function_exists('__fiber_start'),
         function_exists('__fiber_suspend'));
var_dump((new ReflectionMethod('Fiber', 'suspend'))->isStatic());
?>
--EXPECT--
bool(true)
bool(true)
int(1)
int(0)
bool(true)
got:A
int(2)
bool(false)
int(9)
0=>1
got:
1=>2
int(7)
bool(true)
bool(true)
bool(false)
string(6) "done:R"
bool(true)
send 0: ArgumentCountError: Generator::send() expects exactly 1 argument, 0 given
throw 0: ArgumentCountError: Generator::throw() expects exactly 1 argument, 0 given
current 1: ArgumentCountError: Generator::current() expects exactly 0 arguments, 1 given
isStarted 1: ArgumentCountError: Fiber::isStarted() expects exactly 0 arguments, 1 given
new Fiber 0: ArgumentCountError: Fiber::__construct() expects exactly 1 argument, 0 given
bool(false)
bool(false)
bool(false)
bool(true)
--CLEAN--
<?php
