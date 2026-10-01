--TEST--
A capture-less anonymous function re-declared by a second call is not a redeclaration
--FILE--
<?php
// A `static function () {}` with no `use` captures nothing, so this engine does
// not flag it a closure: it is compiled under a synthesized `[lambda_N]` name and
// bound by the same opcode a conditional `function f(){}` is. Running that
// declaration twice is ordinary -- guzzle's `Middleware::redirect()` returns one
// on every call -- and must not be php's redeclaration fatal, which only a name a
// program actually wrote can be involved in.
class AnonFnM {
    static function redirect(): callable { return static function ($h) { return "R$h"; }; }
}
echo AnonFnM::redirect()('a'), AnonFnM::redirect()('b'), "\n";
function anonFnMk() { return static function () { return 1; }; }
echo anonFnMk()(), anonFnMk()(), "\n";
function anonFnArrow() { return fn() => 2; }
echo anonFnArrow()(), anonFnArrow()(), "\n";
function anonFnUse() { $v = 3; return function () use ($v) { return $v; }; }
echo anonFnUse()(), anonFnUse()(), "\n";
$anonFnR = AnonFnM::redirect();
echo $anonFnR instanceof Closure ? "closure" : "not", "\n";
--EXPECT--
RaRb
11
22
33
closure
