--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What a suspension point INSIDE a C builtin's callback still owes: Fiber::throw() lands at it, an uncaught throw leaves the fiber and is re-raised at resume(), an abandoned fiber's half-finished builtin frames unwind, and a resumed one carries the builtin's own loop on to the end.
--FILE--
<?php
// Fiber::throw() lands AT the suspension point, even when that point is inside
// a C builtin's callback.
$f = new Fiber(function () {
    try {
        return array_map(static fn($x) => Fiber::suspend($x), [1])[0];
    } catch (DomainException $e) {
        return 'caught in map: ' . $e->getMessage();
    }
});
echo "start: ", var_export($f->start(), true), "\n";
$f->throw(new DomainException('inj'));
echo "after throw: ", var_export($f->getReturn(), true), "\n";

// An exception the body does not catch leaves the fiber and is re-raised at the
// resume() that ran it -- from a suspension point inside a callback too.
$g = new Fiber(function () {
    $a = [2, 1];
    usort($a, static function ($x, $y) { Fiber::suspend('cmp'); throw new RuntimeException('from comparator'); });
});
echo "usort start: ", var_export($g->start(), true), "\n";
try {
    $g->resume();
} catch (RuntimeException $e) {
    echo "escaped: ", $e->getMessage(), "\n";
}
var_dump($g->isTerminated());

// A fiber abandoned while suspended inside a C callback: its half-finished
// builtin frames are unwound, and the script carries on.
$h = new Fiber(function () {
    return preg_replace_callback('/a/', static fn($m) => Fiber::suspend('p'), 'aaa');
});
echo "preg start: ", var_export($h->start(), true), "\n";
$h = null;
gc_collect_cycles();
echo "abandoned, still here\n";

// ...and one resumed to completion, to show the callback loop really continued.
$i = new Fiber(function () {
    return preg_replace_callback('/a/', static fn($m) => Fiber::suspend('p'), 'aaa');
});
$v = $i->start();
$n = 0;
while ($i->isSuspended()) {
    $i->resume('<' . (++$n) . '>');
}
echo "preg ret: ", var_export($i->getReturn(), true), "\n";
?>
--EXPECT--
start: 1
after throw: 'caught in map: inj'
usort start: 'cmp'
escaped: from comparator
bool(true)
preg start: 'p'
abandoned, still here
preg ret: '<1><2><3>'
--CLEAN--
<?php
