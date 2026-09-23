--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable over a magic method dispatches __call
--DESCRIPTION--
`$o->missing(...)`, `C::missing(...)` and Closure::fromCallable([$o,'missing'])
are method callables, and php runs the class's catch-all through them like any
other spelling. PHL stored only the method NAME in the closure and decided at
call time whether it was a method by asking the class — so a name the class
answers only through __call fell through to the plain-function route and failed
with "Call to undefined function missing()", and a name that happened to match a
global FUNCTION ran the function instead of the catch-all. The closure records
that its name is a method now. A bound plain closure
(`function(){…}->bindTo($o)`) still dispatches its own body, which is what that
guess was really there to protect.
--FILE--
<?php
class CfccMagic {
    private function cfccPriv($a) { return "real($a)"; }
    public function __call($n, $a) { return "__call($n," . implode(',', $a) . ') on ' . get_class($this); }
    public static function __callStatic($n, $a) { return "__callStatic($n," . implode(',', $a) . ')'; }
}
function cfccMissing() { return 'GLOBAL cfccMissing'; }
$o = new CfccMagic;

/* Every method-callable spelling reaches the catch-all. */
echo ($o->cfccMissing(...))(1), "\n";
echo (CfccMagic::cfccMissing(...))(2), "\n";
echo Closure::fromCallable([$o, 'cfccMissing'])(3), "\n";
echo Closure::fromCallable(['CfccMagic', 'cfccMissing'])(4), "\n";
echo Closure::fromCallable('CfccMagic::cfccMissing')(5), "\n";
/* An inaccessible one goes the same way. */
echo ($o->cfccPriv(...))(6), "\n";
echo Closure::fromCallable([$o, 'cfccPriv'])(7), "\n";
/* A global function of the same name does NOT shadow the method callable. */
echo cfccMissing(), "\n";
echo Closure::fromCallable('cfccMissing')(), "\n";

/* A real method is unaffected, and so is a bound plain closure. */
class CfccPlain {
    public $p = 'prop';
    public function real($a) { return "real($a)"; }
}
$q = new CfccPlain;
echo ($q->real(...))(8), "\n";
echo Closure::fromCallable([$q, 'real'])(9), "\n";
$bound = function () { return 'body:' . $this->p; };
echo $bound->bindTo($q)(), "\n";
$boundOnMagic = function () { return 'body-on-magic'; };
echo $boundOnMagic->bindTo($o)(), "\n";

/* Closure::fromCallable says WHY it refuses, with php's reason taxonomy. */
class CfccNone { private function priv() {} public function real() {} }
foreach ([
    fn() => Closure::fromCallable('cfccNoSuchFunction'),
    fn() => Closure::fromCallable([new CfccNone, 'zz']),
    fn() => Closure::fromCallable([new CfccNone, 'priv']),
    fn() => Closure::fromCallable(['CfccNoSuchClass', 'm']),
    fn() => Closure::fromCallable('CfccNone::real'),
    fn() => Closure::fromCallable(5),
    fn() => Closure::fromCallable([1, 2, 3]),
] as $bad) {
    try { $bad(); echo "no error\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo "end\n";
?>
--EXPECT--
__call(cfccMissing,1) on CfccMagic
__callStatic(cfccMissing,2)
__call(cfccMissing,3) on CfccMagic
__callStatic(cfccMissing,4)
__callStatic(cfccMissing,5)
__call(cfccPriv,6) on CfccMagic
__call(cfccPriv,7) on CfccMagic
GLOBAL cfccMissing
GLOBAL cfccMissing
real(8)
real(9)
body:prop
body-on-magic
TypeError: Failed to create closure from callable: function "cfccNoSuchFunction" not found or invalid function name
TypeError: Failed to create closure from callable: class CfccNone does not have a method "zz"
TypeError: Failed to create closure from callable: cannot access private method CfccNone::priv()
TypeError: Failed to create closure from callable: class "CfccNoSuchClass" not found
TypeError: Failed to create closure from callable: non-static method CfccNone::real() cannot be called statically
TypeError: Failed to create closure from callable: no array or string given
TypeError: Failed to create closure from callable: array callback must have exactly two members
end
