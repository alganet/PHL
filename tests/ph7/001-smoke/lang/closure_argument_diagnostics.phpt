--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An argument diagnostic names a closure the way php does, scope and all
--FILE--
<?php
/* php calls a closure `{closure:file:line}`, or `{closure:enclosing():line}` for
 * one written inside a function -- and prefixes the class it belongs to. The four
 * argument diagnostics printed the engine's own `[closure_N]` instead. Paths are
 * normalised to a basename so the expectation is the same on every machine. */
function cadShow(Throwable $e)
{
	/* Every path in the message is this directory's; only the basename is stable. */
	$cadDir = __DIR__;
	echo str_replace([$cadDir . '/', $cadDir . '\\', str_replace('\\', '/', $cadDir) . '/'],
		'', $e->getMessage()), "\n";
}

$cadPlain = function ($a, $b) {};
$cadTyped = function (int $x, $y) {};
$cadRef = function (&$r) {};
$cadHole = function ($a, $b, $c) {};

class CadHost
{
	public function m() { return function (int $x, $y) {}; }
	public static function s() { return function (int $x, $y) {}; }
	public function nested() { return (function () { return function ($a, $b) {}; })(); }
	public function real(int $x, $y) {}
}
class CadOther {}

$cadMeth = (new CadHost())->m();
$cadStatic = CadHost::s();
$cadNested = (new CadHost())->nested();
$cadBound = Closure::bind($cadTyped, null, CadOther::class);

/* Too few arguments, and the CALL SITE php names only when the caller is user code. */
foreach (['plain' => $cadPlain, 'method' => $cadMeth, 'static' => $cadStatic,
          'nested' => $cadNested, 'bound' => $cadBound] as $cadName => $cadFn) {
	try { $cadFn(1); } catch (Throwable $e) { echo "$cadName: "; cadShow($e); }
}
try { (new CadHost())->real(1); } catch (Throwable $e) { echo 'real method: '; cadShow($e); }

/* A TypeError, an unpassed named-argument hole, and a by-reference refusal. */
try { $cadTyped('nope', 1); } catch (Throwable $e) { echo 'type: '; cadShow($e); }
try { $cadMeth('nope', 1); } catch (Throwable $e) { echo 'type method: '; cadShow($e); }
try { $cadHole(a: 1, c: 3); } catch (Throwable $e) { echo 'hole: '; cadShow($e); }
try { $cadRef(5); } catch (Throwable $e) { echo 'byref: '; cadShow($e); }

/* An INTERNAL function reaching for a callback has no calling line to name, so php
 * leaves the `in FILE on line N` segment out entirely. */
try { array_map(function ($a, $b) {}, [1]); } catch (Throwable $e) { echo 'callback: '; cadShow($e); }
try { $cadArr = [2, 1]; usort($cadArr, function ($a, $b, $c) { return 0; }); }
catch (Throwable $e) { echo 'usort: '; cadShow($e); }
try { (new Fiber(function ($a, $b) {}))->start(1); } catch (Throwable $e) { echo 'fiber: '; cadShow($e); }
/* ...but a generator's own resume IS user code. */
try { $cadGen = (function ($a, $b) { yield 1; })(1); $cadGen->current(); }
catch (Throwable $e) { echo 'generator: '; cadShow($e); }
?>
--EXPECT--
plain: Too few arguments to function {closure:closure_argument_diagnostics.phpt.file:14}(), 1 passed in closure_argument_diagnostics.phpt.file on line 36 and exactly 2 expected
method: Too few arguments to function CadHost::{closure:CadHost::m():21}(), 1 passed in closure_argument_diagnostics.phpt.file on line 36 and exactly 2 expected
static: Too few arguments to function CadHost::{closure:CadHost::s():22}(), 1 passed in closure_argument_diagnostics.phpt.file on line 36 and exactly 2 expected
nested: Too few arguments to function CadHost::{closure:{closure:CadHost::nested():23}:23}(), 1 passed in closure_argument_diagnostics.phpt.file on line 36 and exactly 2 expected
bound: Too few arguments to function CadOther::{closure:closure_argument_diagnostics.phpt.file:15}(), 1 passed in closure_argument_diagnostics.phpt.file on line 36 and exactly 2 expected
real method: Too few arguments to function CadHost::real(), 1 passed in closure_argument_diagnostics.phpt.file on line 38 and exactly 2 expected
type: {closure:closure_argument_diagnostics.phpt.file:15}(): Argument #1 ($x) must be of type int, string given, called in closure_argument_diagnostics.phpt.file on line 41
type method: CadHost::{closure:CadHost::m():21}(): Argument #1 ($x) must be of type int, string given, called in closure_argument_diagnostics.phpt.file on line 42
hole: {closure:closure_argument_diagnostics.phpt.file:17}(): Argument #2 ($b) not passed
byref: {closure:closure_argument_diagnostics.phpt.file:16}(): Argument #1 ($r) could not be passed by reference
callback: Too few arguments to function {closure:closure_argument_diagnostics.phpt.file:48}(), 1 passed and exactly 2 expected
usort: Too few arguments to function {closure:closure_argument_diagnostics.phpt.file:49}(), 2 passed and exactly 3 expected
fiber: Too few arguments to function {closure:closure_argument_diagnostics.phpt.file:51}(), 1 passed and exactly 2 expected
generator: Too few arguments to function {closure:closure_argument_diagnostics.phpt.file:53}(), 1 passed in closure_argument_diagnostics.phpt.file on line 53 and exactly 2 expected
