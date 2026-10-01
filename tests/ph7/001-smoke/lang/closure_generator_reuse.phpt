--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure: a Generator or Fiber made from a closure does not free the closure's body
--FILE--
<?php
/* A run-time closure is a per-instantiation function record the Closure object
 * owns. A Generator or a Fiber built from one keeps it alive too, and used to
 * give back one hold more than it took -- so the FIRST coroutine made from a
 * closure freed the body its own Closure was still naming. */

/* A generator closure, called again after the first generator has died. */
$crgClosure = function () { yield 1; yield 2; };
foreach ($crgClosure() as $v) { echo "a$v\n"; }
foreach ($crgClosure() as $v) { echo "b$v\n"; }

/* Never started, then dropped: the close path is the same one. */
$crgUnused = $crgClosure();
unset($crgUnused);
foreach ($crgClosure() as $v) { echo "c$v\n"; }

/* A capture and a $this binding travel with the copy. */
$crgN = 5;
$crgUse = function () use ($crgN) { yield $crgN; };
foreach ($crgUse() as $v) { echo "use$v\n"; }
foreach ($crgUse() as $v) { echo "use$v\n"; }

class CrgHolder implements IteratorAggregate
{
    private $gen;
    public $x = 7;
    public function __construct(Closure $gen) { $this->gen = $gen; }
    public function getIterator(): Traversable { return ($this->gen)(); }
    public function twice(): void
    {
        $f = function () { yield $this->x; };
        foreach ($f() as $v) { echo "m$v\n"; }
        foreach ($f() as $v) { echo "m$v\n"; }
    }
}

/* symfony/console's TableRows: getIterator() re-invokes the same closure once per
 * pass over the rows, and Table::render makes three passes. */
$crgAgg = new CrgHolder(function () { yield 'x'; yield 'y'; });
foreach ($crgAgg as $v) { echo "i$v\n"; }
foreach ($crgAgg as $v) { echo "i$v\n"; }
foreach ($crgAgg as $v) { echo "i$v\n"; }
$crgAgg->twice();

/* An arrow function is a closure too. */
$crgArrow = fn () => yield 42;
foreach ($crgArrow() as $v) { echo "f$v\n"; }
foreach ($crgArrow() as $v) { echo "f$v\n"; }

/* A Fiber holds the same record the same way. */
$crgFiber = function () { Fiber::suspend('s'); return 'done'; };
$a = new Fiber($crgFiber);
echo $a->start(), "\n";
unset($a);
$b = new Fiber($crgFiber);
echo $b->start(), "\n";

/* The closure may die first: the coroutine keeps the body alive. */
$crgOwn = function () { yield 'kept'; };
$crgGen = $crgOwn();
unset($crgOwn);
foreach ($crgGen as $v) { echo "$v\n"; }
?>
--EXPECT--
a1
a2
b1
b2
c1
c2
use5
use5
ix
iy
ix
iy
ix
iy
m7
m7
f42
f42
s
s
kept
--CLEAN--
<?php
unset($crgClosure,$crgUnused,$crgN,$crgUse,$crgAgg,$crgArrow,$crgFiber,$crgOwn,$crgGen,$a,$b,$v);
