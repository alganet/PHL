--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two live activations of one foreach (generators/fibers/recursion) keep private cursors
--FILE--
<?php
/* A foreach step is per-STATEMENT, but two suspended activations of the same
 * textual foreach (two generator instances, a recursive call, or two fibers)
 * must each keep their own cursor — resuming one must not advance onto the
 * other's position. The step is keyed by the running activation's frame. */

// (1) Two interleaved instances of one generator over the same loop.
function fsgTwoInst($a) { foreach ($a as $x) yield $x; }
$g1 = fsgTwoInst([1, 2, 3]);
$g2 = fsgTwoInst([10, 20]);
echo $g1->current(), $g2->current();      // 1 10
$g1->next(); $g2->next();
echo $g1->current(), $g2->current();      // 2 20
$g1->next(); $g2->next();
echo $g1->current();                      // 3   ($g2 is now exhausted)
echo "\n";

// (2) A recursive generator suspended in the same loop as its own outer call.
function fsgRec($a) {
    foreach ($a as $x) {
        if ($x == 1) { $inner = fsgRec([7, 8]); echo $inner->current(); }
        yield $x;
    }
}
foreach (fsgRec([1, 2]) as $v) echo "|$v";  // 7|1|2
echo "\n";

// (3) Two fibers suspended inside the same textual foreach.
$mk = fn() => new Fiber(function () {
    foreach ([100, 200] as $x) Fiber::suspend($x);
});
$f1 = $mk(); $f2 = $mk();
echo $f1->start(), $f2->start();          // 100 100
echo $f1->resume(), $f2->resume();        // 200 200
echo "\n";

// (4) Nested foreach over one array inside a generator still interleaves right.
function fsgNested($a) {
    foreach ($a as $x) foreach ($a as $y) yield "$x$y";
}
$n1 = fsgNested([1, 2]);
$n2 = fsgNested([3, 4]);
echo $n1->current(), "-", $n2->current();  // 11-33
$n1->next();
echo $n1->current(), "-", $n2->current();  // 12-33
echo "\n";

// (5) Plain single-instance generator loop is unaffected.
function fsgPlain() { foreach ([1, 2, 3] as $x) yield $x; }
foreach (fsgPlain() as $v) echo $v;        // 123
echo "\n";

// (6) break inside the generator's foreach leaves the step behind (no unwind
// pop); a second live instance must still keep its own cursor, and the step
// teardown must not reorder a live step below the leaked one.
function fsgBreak($a) {
    foreach ($a as $x) {
        if ($x == 99) break;   // never taken here — keeps the loop's step live
        yield $x;
    }
}
$b1 = fsgBreak([1, 2, 3]);
$b2 = fsgBreak([10, 20]);
echo $b1->current(), $b2->current();       // 1 10
$b1->next();
echo $b1->current(), $b2->current();       // 2 10
echo "\n";

// (7) An inner loop that actually breaks (leaking its step) inside an outer
// foreach over a generator — the outer cursor is unaffected.
function fsgYield($a) { foreach ($a as $x) yield $x; }
$acc = "";
foreach (fsgYield([1, 2, 3]) as $v) {
    foreach (fsgYield([7, 8, 9]) as $w) { if ($w == 8) break; }
    $acc .= "$v$w ";                       // $w is 8 at break
}
echo $acc;                                 // 18 28 38
echo "\n";
?>
--EXPECT--
1102203
7|1|2
100100200200
11-3312-33
123
110210
18 28 38
--CLEAN--
<?php
