--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator abandoned (unset / out of scope) before completion runs its pending finally blocks, PHP-exact
--FILE--
<?php
// unset() before completion runs the finally of the suspended try
function g1() { try { yield 1; yield 2; } finally { echo "fin1\n"; } }
$x = g1(); echo $x->current(), "\n"; unset($x); echo "after1\n";

// going out of scope (function return) runs it too
function g2() { try { yield 1; } finally { echo "fin2\n"; } }
function h() { $y = g2(); $y->current(); }
h(); echo "after2\n";

// a never-started generator has no open try -> no finally
function g3() { try { yield 1; } finally { echo "fin3\n"; } }
$z = g3(); unset($z); echo "after3\n";

// nested try/finally: innermost finally first, then the outer
function g4() {
    try { try { yield 1; } finally { echo "inner\n"; } }
    finally { echo "outer\n"; }
}
$a = g4(); $a->current(); unset($a); echo "after4\n";

// catch is NOT run on close; only finally
function g5() {
    try { yield 1; } catch (Exception $e) { echo "catch\n"; } finally { echo "fin5\n"; }
}
$b = g5(); $b->current(); unset($b); echo "after5\n";

// a return inside the finally during close is discarded, the finally still runs
function g6() { try { yield 1; } finally { echo "fin6\n"; return 99; } }
$c = g6(); $c->current(); unset($c); echo "after6\n";

// local state is available to the finally
function g7() { $r = "handle"; try { yield 1; } finally { echo "closing $r\n"; } }
$d = g7(); $d->current(); unset($d); echo "after7\n";

// closing a `yield from` delegator closes the inner delegate FIRST (innermost-first)
function inner8() { try { yield 1; yield 2; } finally { echo "inner8\n"; } }
function outer8() { try { yield from inner8(); } finally { echo "outer8\n"; } }
$e = outer8(); $e->current(); unset($e); echo "after8\n";

// same innermost-first close order when the delegate is an IteratorAggregate whose
// getIterator() returns a generator (delegation state 2, not a direct Generator)
class Agg9 implements IteratorAggregate {
    public function getIterator(): Iterator {
        return (function () { try { yield 1; yield 2; } finally { echo "inner9\n"; } })();
    }
}
function outer9() { try { yield from new Agg9(); } finally { echo "outer9\n"; } }
$f = outer9(); $f->current(); unset($f); echo "after9\n";
?>
--EXPECT--
1
fin1
after1
fin2
after2
after3
inner
outer
after4
fin5
after5
fin6
after6
closing handle
after7
inner8
outer8
after8
inner9
outer9
after9
