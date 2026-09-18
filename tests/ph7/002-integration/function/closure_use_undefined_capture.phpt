--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-value closure use() of an undefined variable warns AT CLOSURE CREATION, at the capture's line
--DESCRIPTION--
php reads a by-value `use ($q)` capture when the closure is created and raises E_WARNING
"Undefined variable $q" right there (before the body ever runs), capturing NULL, and attributes
it to the capture's OWN source line — which differs from the closure keyword's line when the
use-clause wraps. The by-ref form `use (&$q)` stays silent because it creates the binding. The
auto-captured $this of a method closure never warns, and an arrow function's implicit captures
warn only when the body reads them, not at creation. PHL used to capture silently in every case.
The warning prefix and stream differ across engines, so the handler normalizes them (see the §
stdout/stderr routing item); the 4th handler arg pins the line.
--FILE--
<?php
// The 4th arg pins the line php attributes the warning to — the capture's own
// source line, which the handler normalizes across engines (prefix/stream aside).
set_error_handler(function ($no, $str, $file, $line) { echo "[$no L$line] $str\n"; return true; });

echo "== by-value undefined: warns at creation ==\n";
echo "before\n";
$f = function () use ($cuu_missing) { return 1; };
echo "after\n";

echo "== by-value defined: silent ==\n";
$cuu_have = 7;
$g = function () use ($cuu_have) { return $cuu_have; };
echo $g(), "\n";

echo "== by-ref undefined: silent ==\n";
$h = function () use (&$cuu_ref) { return 2; };
echo "still here\n";

echo "== mixed defined + undefined ==\n";
$cuu_a = 1;
$m = function () use ($cuu_a, $cuu_b) { return 3; };
echo "done\n";

echo "== static closure undefined: warns ==\n";
$s = static function () use ($cuu_stat) { return 4; };
echo "ok\n";

echo "== wrapped use clause: each capture warns at its OWN line ==\n";
$cuu_mid = 1;
$w = function () use (
    $cuu_top,
    $cuu_mid,
    $cuu_bot
) { return 5; };
echo "wrapped\n";

echo "== arrow fn undefined, body not run: silent at creation ==\n";
$arrow = fn () => $cuu_arrow;
echo "arrow made\n";

echo "== method closure auto-\$this: no spurious warn ==\n";
class CuuHolder {
    public $v = 9;
    function make() { return function () { return $this->v; }; }
}
$obj = new CuuHolder();
$cm = $obj->make();
echo $cm(), "\n";
?>
--EXPECT--
== by-value undefined: warns at creation ==
before
[2 L8] Undefined variable $cuu_missing
after
== by-value defined: silent ==
7
== by-ref undefined: silent ==
still here
== mixed defined + undefined ==
[2 L22] Undefined variable $cuu_b
done
== static closure undefined: warns ==
[2 L26] Undefined variable $cuu_stat
ok
== wrapped use clause: each capture warns at its OWN line ==
[2 L32] Undefined variable $cuu_top
[2 L34] Undefined variable $cuu_bot
wrapped
== arrow fn undefined, body not run: silent at creation ==
arrow made
== method closure auto-$this: no spurious warn ==
9
