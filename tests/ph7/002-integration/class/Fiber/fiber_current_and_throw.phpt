--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fiber::getCurrent() and Fiber::throw(), and what a terminated fiber answers
--DESCRIPTION--
Two of php's eleven Fiber methods were missing. `getCurrent()` is the one a program
calls without holding a fiber at all -- it is how code asks whether it is inside one,
and monolog's Logger asks it on EVERY record it writes, so its whole suite died on
`Call to undefined method Fiber::getCurrent()`. It answers php's EG(active_fiber): the
fiber whose BODY the running code is inside, which is not the innermost coroutine (a
generator iterated inside a fiber leaves the fiber current) and not the fiber a
suspended one returned to. `throw()` resumes a fiber by RAISING at its suspension
point, so `Fiber::suspend()` throws instead of returning -- over the same
inject-at-resume transport Generator::throw() uses, including for a fiber suspended
DEEP inside a nested call, where the raise belongs at the parked callee rather than
at the body.

Three answers a terminated fiber gives came with them: a fiber whose body let an
exception escape IS terminated (php's DEAD state, which PHL reported only for a body
that RETURNED), its getReturn() says it threw rather than that it has not returned,
and a fiber that was never started names that instead of answering null.
--FILE--
<?php
function t(string $label, callable $fn): void {
    try { $r = $fn(); echo $label, ' => '; var_dump($r); }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "== Fiber::getCurrent() names the fiber the code is INSIDE ==\n";
var_dump(Fiber::getCurrent());
$inner = null;
$f = new Fiber(function () use (&$inner) {
    $inner = Fiber::getCurrent();
    var_dump($inner === Fiber::getCurrent());
    /* a generator iterated inside a fiber leaves the FIBER current */
    foreach ((function () { yield 1; yield 2; })() as $v) {
        var_dump(Fiber::getCurrent() !== null);
    }
    Fiber::suspend('paused');
    var_dump(Fiber::getCurrent() !== null);
    return 'done';
});
var_dump($f->start());
var_dump($inner === $f);
var_dump(Fiber::getCurrent());
var_dump($f->resume());
var_dump($f->getReturn());
var_dump(Fiber::getCurrent());

echo "== and it nests with the calls ==\n";
$outer = new Fiber(function () {
    $o = Fiber::getCurrent();
    (new Fiber(function () use ($o) { var_dump(Fiber::getCurrent() !== $o); }))->start();
    var_dump(Fiber::getCurrent() === $o);
});
$outer->start();
var_dump(Fiber::getCurrent());
$boom = new Fiber(function () { throw new RuntimeException('x'); });
try { $boom->start(); } catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump(Fiber::getCurrent());

echo "== Fiber::throw() raises at the suspension point ==\n";
$a = new Fiber(function () { $v = Fiber::suspend('s1'); echo "NOT REACHED\n"; return 'end'; });
t('throw before start', fn() => $a->throw(new RuntimeException('early')));
t('start', fn() => $a->start());
t('throw into suspended', fn() => $a->throw(new RuntimeException('boom')));
t('terminated after', fn() => $a->isTerminated());
t('suspended after', fn() => $a->isSuspended());
t('started after', fn() => $a->isStarted());
t('running after', fn() => $a->isRunning());
t('getReturn after', fn() => $a->getReturn());
t('throw again', fn() => $a->throw(new RuntimeException('again')));

echo "== the fiber's OWN try/catch catches it and carries on ==\n";
$b = new Fiber(function () {
    try { Fiber::suspend('x'); } catch (LogicException $e) { echo 'caught inside: ', $e->getMessage(), "\n"; return 'recovered'; }
    return 'no';
});
t('start b', fn() => $b->start());
t('throw caught inside', fn() => $b->throw(new LogicException('inner')));
t('b return', fn() => $b->getReturn());

echo "== a DEEP suspend raises where the suspend is ==\n";
$c = new Fiber(function () {
    $g = function () { return Fiber::suspend('deep'); };
    try { $g(); } catch (DomainException $e) { echo 'deep caught: ', $e->getMessage(), "\n"; return 'deep-ok'; }
    return 'no';
});
t('start c', fn() => $c->start());
t('throw into deep', fn() => $c->throw(new DomainException('d')));
t('c return', fn() => $c->getReturn());

echo "== a fiber that never started, and one that threw ==\n";
$d = new Fiber(fn() => 1);
t('getReturn, never started', fn() => $d->getReturn());
t('isTerminated, never started', fn() => $d->isTerminated());
$e2 = new Fiber(function () { Fiber::suspend(1); return 2; });
$e2->start();
t('getReturn, suspended', fn() => $e2->getReturn());
t('bad throw argument', fn() => (new Fiber(fn() => 1))->throw(5));

echo "== the method itself ==\n";
$m = new ReflectionMethod('Fiber', 'getCurrent');
var_dump($m->isStatic(), $m->isPublic(), $m->getNumberOfParameters(), (string)$m->getReturnType());
$m2 = new ReflectionMethod('Fiber', 'throw');
var_dump($m2->isStatic(), $m2->isPublic(), $m2->getNumberOfParameters(), (string)$m2->getReturnType());
var_dump(Fiber::getCurrent(...) instanceof Closure, call_user_func(['Fiber', 'getCurrent']));

echo "done\n";
?>
--EXPECT--
== Fiber::getCurrent() names the fiber the code is INSIDE ==
NULL
bool(true)
bool(true)
bool(true)
string(6) "paused"
bool(true)
NULL
bool(true)
NULL
string(4) "done"
NULL
== and it nests with the calls ==
bool(true)
bool(true)
NULL
RuntimeException
NULL
== Fiber::throw() raises at the suspension point ==
throw before start => FiberError: Cannot resume a fiber that is not suspended
start => string(2) "s1"
throw into suspended => RuntimeException: boom
terminated after => bool(true)
suspended after => bool(false)
started after => bool(true)
running after => bool(false)
getReturn after => FiberError: Cannot get fiber return value: The fiber threw an exception
throw again => FiberError: Cannot resume a fiber that is not suspended
== the fiber's OWN try/catch catches it and carries on ==
start b => string(1) "x"
caught inside: inner
throw caught inside => NULL
b return => string(9) "recovered"
== a DEEP suspend raises where the suspend is ==
start c => string(4) "deep"
deep caught: d
throw into deep => NULL
c return => string(7) "deep-ok"
== a fiber that never started, and one that threw ==
getReturn, never started => FiberError: Cannot get fiber return value: The fiber has not been started
isTerminated, never started => bool(false)
getReturn, suspended => FiberError: Cannot get fiber return value: The fiber has not returned
bad throw argument => TypeError: Fiber::throw(): Argument #1 ($exception) must be of type Throwable, int given
== the method itself ==
bool(true)
bool(true)
int(0)
string(6) "?Fiber"
bool(false)
bool(true)
int(1)
string(5) "mixed"
bool(true)
NULL
done
