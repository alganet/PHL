--TEST--
Closure by-reference capture use (&$x): recursion idiom, write-through, lifetime, generators, fibers
--FILE--
<?php
// Canonical recursion idiom
$cbrFact = function ($n) use (&$cbrFact) { return $n <= 1 ? 1 : $n * $cbrFact($n - 1); };
echo $cbrFact(5), "\n";
// Writes inside the closure are visible outside, and vice versa
$cbrX = 1;
$cbrInc = function () use (&$cbrX) { $cbrX++; };
$cbrInc(); $cbrInc();
echo $cbrX, "\n";
$cbrY = 1;
$cbrGet = function () use (&$cbrY) { return $cbrY; };
$cbrY = 5;
echo $cbrGet(), "\n";
// By-value capture still snapshots
$cbrZ = 1;
$cbrSnap = function () use ($cbrZ) { return $cbrZ; };
$cbrZ = 9;
echo $cbrSnap(), "\n";
// Closure outlives its creating frame
function cbrMkCounter() { $c = 0; return function () use (&$c) { return ++$c; }; }
$cbrF = cbrMkCounter();
$cbrG = cbrMkCounter();
echo $cbrF(), $cbrF(), $cbrF(), $cbrG(), "\n";
// Mixed by-ref and by-value in one use list
$cbrA = 1; $cbrB = 2; $cbrC = 3;
$cbrMix = function () use (&$cbrA, $cbrB, &$cbrC) { $cbrA += 10; $cbrC += 100; return "$cbrA|$cbrB|$cbrC"; };
echo $cbrMix(), ";", $cbrA, ",", $cbrB, ",", $cbrC, "\n";
// Two closures share one variable
$cbrN = 0;
$cbrBump = function () use (&$cbrN) { $cbrN++; };
$cbrRead = function () use (&$cbrN) { return $cbrN; };
$cbrBump(); $cbrBump(); $cbrBump();
echo $cbrRead(), "\n";
// Capture of a not-yet-defined variable auto-vivifies null, assignable through the closure
$cbrSet = function ($v) use (&$cbrLate) { $cbrLate = $v; };
$cbrSet(42);
echo $cbrLate, "\n";
// Mutual recursion through two by-ref captures
$cbrEven = null; $cbrOdd = null;
$cbrEven = function ($n) use (&$cbrOdd) { return $n == 0 ? true : $cbrOdd($n - 1); };
$cbrOdd = function ($n) use (&$cbrEven) { return $n == 0 ? false : $cbrEven($n - 1); };
echo $cbrEven(10) ? "T" : "F", $cbrOdd(7) ? "T" : "F", "\n";
// Array captured by reference
$cbrArr = [1, 2];
$cbrPush = function ($v) use (&$cbrArr) { $cbrArr[] = $v; };
$cbrPush(3); $cbrPush(4);
echo implode(",", $cbrArr), "\n";
// Nested closures chaining the same reference
$cbrW = 5;
$cbrOuter = function () use (&$cbrW) {
    $cbrInner = function () use (&$cbrW) { $cbrW *= 2; };
    $cbrInner();
    return $cbrW;
};
echo $cbrOuter(), ",", $cbrW, "\n";
// Closures created in a loop share the loop variable; by-value snapshots per-iteration
$cbrFns = [];
for ($cbrI = 0; $cbrI < 3; $cbrI++) { $cbrFns[] = function () use (&$cbrI) { return $cbrI; }; }
echo $cbrFns[0](), $cbrFns[1](), $cbrFns[2](), "\n";
$cbrFns = [];
for ($cbrI = 0; $cbrI < 3; $cbrI++) { $cbrFns[] = function () use ($cbrI) { return $cbrI; }; }
echo $cbrFns[0](), $cbrFns[1](), $cbrFns[2](), "\n";
// $this and by-ref capture coexist in a method-made closure
class CbrHolder {
    public $v = 5;
    function make() {
        $cnt = 0;
        return function () use (&$cnt) { $cnt++; return $this->v + $cnt; };
    }
}
$cbrH = new CbrHolder();
$cbrM = $cbrH->make();
echo $cbrM(), $cbrM(), "\n";
// Closure::bind keeps the by-ref binding live
class CbrPriv { private $p = 10; }
$cbrK = 1;
$cbrCl = function () use (&$cbrK) { return $this->p + $cbrK; };
$cbrBound = Closure::bind($cbrCl, new CbrPriv(), CbrPriv::class);
$cbrK = 2;
echo $cbrBound(), "\n";
// Builtin callback dispatch writes through
$cbrTotal = 0;
array_map(function ($x) use (&$cbrTotal) { $cbrTotal += $x; }, [1, 2, 3, 4]);
echo $cbrTotal, "\n";
$cbrSum = 0;
$cbrAdd = function ($v) use (&$cbrSum) { $cbrSum += $v; };
call_user_func($cbrAdd, 5);
call_user_func_array($cbrAdd, [7]);
echo $cbrSum, "\n";
// Generator closure observes live updates to the shared variable
$cbrT = 0;
$cbrGen = function () use (&$cbrT) { while ($cbrT < 3) { yield $cbrT; } };
foreach ($cbrGen() as $v) { $cbrT++; echo $v; }
echo "|", $cbrT, "\n";
// Fiber closure writes through across suspend/resume
$cbrFs = 0;
$cbrFib = new Fiber(function () use (&$cbrFs) { $cbrFs = 11; Fiber::suspend(); $cbrFs = 22; });
$cbrFib->start();
echo $cbrFs, ",";
$cbrFib->resume();
echo $cbrFs, "\n";
// By-ref parameter chained into a by-ref capture (slot owned by the caller)
function cbrWrap(&$y) { return function () use (&$y) { return ++$y; }; }
$cbrZz = 1;
$cbrQ = cbrWrap($cbrZz);
echo $cbrQ(), $cbrQ(), ",", $cbrZz, "\n";
// Independent instantiations do not share state
function cbrMkPair() { $n = 0; return [function () use (&$n) { return ++$n; }, function () use (&$n) { return $n * 10; }]; }
[$cbrI1, $cbrT1] = cbrMkPair();
[$cbrI2, $cbrT2] = cbrMkPair();
$cbrI1(); $cbrI1();
echo $cbrT1(), ",", $cbrT2(), "\n";
// ReflectionFunction::getClosureUsedVariables reports the live value
$cbrRv = 5;
$cbrRc = function () use (&$cbrRv) {};
$cbrRv = 8;
$cbrUsed = (new ReflectionFunction($cbrRc))->getClosureUsedVariables();
echo $cbrUsed["cbrRv"], "\n";
--EXPECT--
120
3
5
1
1231
11|2|103;11,2,103
3
42
TT
1,2,3,4
10,10
333
012
67
12
10
12
012|3
11,22
23,3
20,0
8
