--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Closure names itself, including a "C::m" string and a magic-method trampoline
--FILE--
<?php
/* A Closure describes ITSELF from what it carries, not from a function record
 * something else has to resolve. Two spellings had none: `"C::m"` (which php
 * treats exactly like `[C, 'm']`) and a name the class reaches only through
 * __call/__callStatic, where php makes a trampoline and still names it. */
class CloR
{
    public function m($x) { return "m:$x"; }
    public static function s($x) { return "s:$x"; }
    public function __call($n, $a) { return "c:$n"; }
    public static function __callStatic($n, $a) { return "cs:$n"; }
    public function __invoke() { return 'inv'; }
}
$cloShow = function (string $tag, $callable) {
    try {
        $c = Closure::fromCallable($callable);
        $r = new ReflectionFunction($c);
        printf("%-16s call=%-8s name=%-14s scope=%-6s this=%s params=%d closure-ish=%s\n",
            $tag, var_export($c(1), true), var_export($r->name ?? '<unset>', true),
            var_export($r->getClosureScopeClass()?->getName(), true),
            var_export($r->getClosureThis() !== null, true),
            $r->getNumberOfParameters(),
            var_export(str_contains($r->name ?? '', '{closure'), true));
    } catch (Throwable $e) {
        printf("%-16s %s: %s\n", $tag, get_class($e), $e->getMessage());
    }
};
$cloObj = new CloR();
$cloShow('string C::s', 'CloR::s');
$cloShow('array [C,s]', ['CloR', 's']);
$cloShow('array [o,m]', [$cloObj, 'm']);
$cloShow('magic [o,zz]', [$cloObj, 'zz']);
$cloShow('magic [C,zz]', ['CloR', 'zz']);
$cloShow('magic C::zz', 'CloR::zz');
$cloShow('invokable', $cloObj);
$cloShow('plain fn', 'clo_plain');
function clo_plain($x) { return "p:$x"; }

/* The refusals php gives for a string that names nothing callable. */
foreach (['CloR::nope2', 'NoSuchClass::m'] as $cloBad) {
    try { Closure::fromCallable($cloBad); }
    catch (Throwable $e) { echo 'bad ', $cloBad, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
/* A private one is refused at creation, as php does. */
class CloP { private static function p() {} }
try { Closure::fromCallable('CloP::p'); }
catch (Throwable $e) { echo 'private => ', get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
string C::s      call='s:1'    name='s'            scope='CloR' this=false params=1 closure-ish=false
array [C,s]      call='s:1'    name='s'            scope='CloR' this=false params=1 closure-ish=false
array [o,m]      call='m:1'    name='m'            scope='CloR' this=true params=1 closure-ish=false
magic [o,zz]     call='c:zz'   name='zz'           scope='CloR' this=true params=0 closure-ish=false
magic [C,zz]     call='cs:zz'  name='zz'           scope='CloR' this=false params=0 closure-ish=false
magic C::zz      call='cs:zz'  name='zz'           scope='CloR' this=false params=0 closure-ish=false
invokable        call='inv'    name='__invoke'     scope='CloR' this=true params=0 closure-ish=false
plain fn         call='p:1'    name='clo_plain'    scope=NULL   this=false params=1 closure-ish=false
bad NoSuchClass::m => TypeError: Failed to create closure from callable: class "NoSuchClass" not found
private => TypeError: Failed to create closure from callable: cannot access private method CloP::p()
