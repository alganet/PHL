--TEST--
static function(){} closures, auto-$this capture without use(), and bound-$this precedence
--FILE--
<?php
// static closure expression parses and calls
$sctA = static function () { return 7; };
echo $sctA(), "\n";
// static closure with a by-ref use list
$sctN = 1;
$sctB = static function () use (&$sctN) { return ++$sctN; };
echo $sctB(), $sctN, "\n";
// bare statement-position static closure is accepted (useless but valid)
static function () {};
static fn() => 1;
echo "stmt-ok", "\n";
// a no-use closure made in a method binds $this
class SctHolder {
    public $v = 7;
    function mk() { return function () { return $this->v; }; }
    function mkStatic() { return static function () { return isset($this) ? "y" : "n"; }; }
}
$sctO = new SctHolder();
$sctF = $sctO->mk();
echo $sctF(), "\n";
// a static closure made in a method has no $this
$sctS = $sctO->mkStatic();
echo $sctS(), "\n";
// global-scope closure has no $this either way
$sctP = function () { return isset($this) ? "y" : "n"; };
echo $sctP(), "\n";
// isStatic() reflection
echo (int) (new ReflectionFunction($sctA))->isStatic(),
     (int) (new ReflectionFunction($sctF))->isStatic(),
     (int) (new ReflectionFunction(static fn() => 1))->isStatic(), "\n";
// binding an instance to a static closure is refused (warning swallowed for parity:
// php fires the handler regardless of error_reporting, PHL gates on it — cover both)
set_error_handler(function () { return true; });
error_reporting(0);
$sctR = Closure::bind($sctA, new SctHolder());
restore_error_handler();
error_reporting(E_ALL);
echo $sctR === null ? "null" : "bound", "\n";
// bindTo(null, scope) keeps the closure callable and grants scope visibility
class SctPriv { private static $sec = 42; }
$sctC = function () { return SctPriv::$sec ?? "no"; };
$sctD = $sctC->bindTo(null, SctPriv::class);
echo $sctD(), "\n";
// an explicit bound $this wins over the creation-time captured $this
class SctOther { public $v = 6; }
$sctE = Closure::bind($sctF, new SctOther(), SctOther::class);
echo $sctE(), $sctF(), "\n";
// ... including through generator and fiber dispatch
class SctGen {
    public $v = 4;
    function mk() { return function () { yield $this->v; }; }
}
$sctG = (new SctGen())->mk();
$sctH = Closure::bind($sctG, new SctOther(), SctOther::class);
foreach ($sctH() as $x) { echo $x; }
foreach ($sctG() as $x) { echo $x; }
echo "\n";
$sctFibC = (new SctHolder())->mk();
$sctFibB = Closure::bind($sctFibC, new SctOther(), SctOther::class);
$sctFib = new Fiber(function () use ($sctFibB, $sctFibC) { echo $sctFibB(), $sctFibC(); });
$sctFib->start();
echo "\n";
--EXPECT--
7
22
stmt-ok
7
n
n
101
null
42
67
64
67
