--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The engine's __call/__callStatic routing is not a callable function
--DESCRIPTION--
Routing `$o->missing()` through __call is the ENGINE's own dispatch, and php
gives it no name a script can reach. PHL used to spell it as a hidden global
host function: the four OP_MEMBER sites wrote the string "__phl_magic_call"
into the callee slot and let the ordinary name lookup find it, so
function_exists() reported it, get_defined_functions() listed it, and calling
it by hand ran an engine internal. The slot carries a MARK now and no name is
involved. Every routing site — a missing method, an inaccessible one, and the
static twin of each — still dispatches, and a throw from the handler still
lands in the enclosing catch mid-expression.
--FILE--
<?php
class CedMagic {
    private function cedPriv($a) { return "real-priv($a)"; }
    private static function cedPrivStatic($a) { return "real-priv-static($a)"; }
    public function __call($name, $args) { return "call:$name(" . implode(',', $args) . ')'; }
    public static function __callStatic($name, $args) { return "static:$name(" . implode(',', $args) . ')'; }
}
class CedThrower {
    public function __call($name, $args) { throw new RuntimeException("boom:$name"); }
    public static function __callStatic($name, $args) { throw new LogicException("sboom:$name"); }
}

/* The dispatch has no name. */
var_dump(function_exists('__phl_magic_call'));
$defined = array_merge(get_defined_functions()['internal'], get_defined_functions()['user']);
var_dump(in_array('__phl_magic_call', $defined, true));
try { __phl_magic_call(); } catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* All four routing sites still dispatch. */
$o = new CedMagic;
echo $o->cedMissing(1, 2), "\n";      // missing instance method
echo $o->cedPriv(3), "\n";            // inaccessible instance method
echo CedMagic::cedMissingS(4), "\n";  // missing static method
echo CedMagic::cedPrivStatic(5), "\n";// inaccessible static method

/* Arguments reach the handler whatever shape the call site has. */
$spread = [6, 7];
echo $o->cedSpread(...$spread), "\n";
$dyn = 'cedDynamic';
echo $o->$dyn(8), "\n";
echo CedMagic::$dyn(9), "\n";
echo $o->cedOuter($o->cedInner(10)), "\n";

/* A throw from the handler unwinds mid-expression, and the try body does not resume. */
$t = new CedThrower;
$kept = 'prior';
try { $kept = 'x' . $t->cedBoom(); echo "resumed\n"; }
catch (RuntimeException $e) { echo 'caught:', $e->getMessage(), "\n"; }
echo 'kept=', $kept, "\n";
try { echo 1 + CedThrower::cedSBoom(); }
catch (LogicException $e) { echo 'caught:', $e->getMessage(), "\n"; }

/* Repeated dispatch: nothing is left latched between calls. */
$acc = [];
for ($i = 0; $i < 3; $i++) { $acc[] = $o->cedLoop($i); }
echo implode('|', $acc), "\n";
echo "end\n";
?>
--EXPECT--
bool(false)
bool(false)
Error: Call to undefined function __phl_magic_call()
call:cedMissing(1,2)
call:cedPriv(3)
static:cedMissingS(4)
static:cedPrivStatic(5)
call:cedSpread(6,7)
call:cedDynamic(8)
static:cedDynamic(9)
call:cedOuter(call:cedInner(10))
caught:boom:cedBoom
kept=prior
caught:sboom:cedSBoom
call:cedLoop(0)|call:cedLoop(1)|call:cedLoop(2)
end
