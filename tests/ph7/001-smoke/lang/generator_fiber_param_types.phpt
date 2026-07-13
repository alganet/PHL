--TEST--
Generator and fiber parameter type hints are enforced at binding time
--DESCRIPTION--
php binds + type-checks generator arguments eagerly at the g(...) call site
(before any resume) and fiber arguments at Fiber::start(). Pre-fix, the
generator/fiber frame installer only cast on mismatch, so g(int $x) given
null silently yielded int(0). Messages are asserted up to " given" (the
", called in %s on line %d" suffix is a recorded engine divergence).
--FILE--
<?php
function gfpt_msg(TypeError $e): string {
    $m = $e->getMessage();
    return substr($m, 0, strpos($m, " given") + 6);
}

// 1. null to a non-nullable int param: TypeError at the call, before any resume
function gfpt_int(int $x) { yield $x; }
try { $g = gfpt_int(null); echo "made\n"; }
catch (TypeError $e) { echo gfpt_msg($e), "\n"; }

// 2. explicit null with a default is NOT redirected to the default
function gfpt_def(int $x = 5) { yield $x; }
try { $g = gfpt_def(null); echo "made\n"; }
catch (TypeError $e) { echo "TE-default\n"; }

// 3. nullable / implicit-nullable params keep null (no cast to 0)
function gfpt_nullable(?int $x = null) { yield $x; }
foreach (gfpt_nullable(null) as $v) { echo var_export($v, true), "\n"; }
foreach (gfpt_nullable() as $v) { echo var_export($v, true), "\n"; }

// 4. weak mode coerces a numeric string
function gfpt_weak(int $x) { yield $x; }
foreach (gfpt_weak("42") as $v) { echo var_export($v, true), "\n"; }

// 5. non-numeric string is a TypeError
try { foreach (gfpt_weak("abc") as $v) {} }
catch (TypeError $e) { echo gfpt_msg($e), "\n"; }

// 6. interface-typed param rejects a non-implementer
interface GfptI {}
function gfpt_iface(GfptI $x) { yield $x; }
try { $g = gfpt_iface(new stdClass()); echo "made\n"; }
catch (TypeError $e) { echo "TE-iface\n"; }

// 7. method generator: message carries Class::method
class GfptC { public function m(int $x) { yield $x; } }
try { (new GfptC())->m(null); }
catch (TypeError $e) { echo gfpt_msg($e), "\n"; }

// 8. typed variadic collects into an array (and coerces elements weakly)
function gfpt_var(int ...$xs) { yield array_sum($xs); }
foreach (gfpt_var(1, "2", 3) as $v) { echo $v, "\n"; }
function gfpt_var2(...$xs) { yield count($xs); }
foreach (gfpt_var2() as $v) { echo $v, "\n"; }

// 9. fiber params: TypeError on null, weak coercion on numeric string
$f = new Fiber(function (int $x) { Fiber::suspend($x); });
try { $f->start(null); echo "started\n"; }
catch (TypeError $e) { echo "TE-fiber\n"; }
$f2 = new Fiber(function (int $x) { Fiber::suspend($x + 1); });
echo $f2->start("41"), "\n";

// 10. the binding TypeError is catchable by the CALLING generator's own
// (inline) try — as is a builtin's throw raised inside that try
function gfpt_outer() {
    try { $g = gfpt_int(null); echo "made-inline\n"; }
    catch (TypeError $e) { echo "caught-inline\n"; }
    try { explode("", ""); }
    catch (ValueError $e) { echo "caught-ve\n"; }
    try { usort($GLOBALS['gfpt_arr'], function ($a, $b) { throw new Exception("u"); }); }
    catch (Exception $e) { echo "caught-cb\n"; }
    yield 1;
}
$gfpt_arr = [3, 1];
foreach (gfpt_outer() as $v) { echo "v=", $v, "\n"; }
echo "done\n";
?>
--EXPECT--
gfpt_int(): Argument #1 ($x) must be of type int, null given
TE-default
NULL
NULL
42
gfpt_weak(): Argument #1 ($x) must be of type int, string given
TE-iface
GfptC::m(): Argument #1 ($x) must be of type int, null given
6
0
TE-fiber
42
caught-inline
caught-ve
caught-cb
v=1
done
