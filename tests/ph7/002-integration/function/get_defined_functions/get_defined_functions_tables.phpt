--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_functions() reports the script's functions, not the engine's table
--DESCRIPTION--
php answers two lists of NAMES, both folded to lower case, the internal one in
registration order and the user one in declaration order. PHL answered its own
compiled-function table instead: every mounted class METHOD was in the "user"
list under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c
mints, and every closure under `[closure_N]` — 763 entries for an empty script,
one more per `function(){}` literal. Under them, the ~28 builtins this engine
happens to implement as embedded PHP (scandir, glob, checkdate, hex2bin, the
class_* trio, preg_grep …) were reported as USER functions, where php — which
has no notion of where an engine implements a builtin — reports them internal.
Names came back in the DECLARED spelling and in reverse order.
--FILE--
<?php
function gdfAlpha() {}
function GdfBeta() {}
function gdf_gamma() {}

class GdfCarrier {
    public function meth() {}
    public static function statMeth() {}
    public function __call($n, $a) {}
}
interface GdfFace { public function ifaceMeth(); }
trait GdfTrait { public function traitMeth() {} }

$closure = function () {};
$arrow = fn() => 1;
$fcc = strlen(...);

$d = get_defined_functions();

/* The two buckets and nothing else. */
var_dump(array_keys($d));

/* The user list is exactly what this file declared, folded, in declaration order. */
var_dump($d['user']);

/* Nothing the engine keyed under a name of its own leaks into either list. */
$all = array_merge($d['internal'], $d['user']);
$engine = array_values(array_filter($all, fn($n) => str_contains($n, '[') || str_contains($n, '@')));
var_dump($engine);

/* Every name is folded, and neither list repeats one. */
var_dump($all === array_map('strtolower', $all));
var_dump(count($all) === count(array_unique($all)));

/* A builtin is internal wherever the engine chose to implement it. */
foreach (['strlen', 'scandir', 'glob', 'checkdate', 'hex2bin', 'class_uses', 'preg_grep'] as $n) {
    printf("%-10s internal=%d user=%d\n", $n,
        (int) in_array($n, $d['internal'], true),
        (int) in_array($n, $d['user'], true));
}

/* Declaring more only appends to the user list. */
eval('function gdfDelta() {}');
$after = get_defined_functions()['user'];
var_dump(array_values(array_diff($after, $d['user'])));

/* $exclude_disabled changes nothing here: no function is disabled. */
var_dump(get_defined_functions(false) === get_defined_functions(true));
?>
--EXPECT--
array(2) {
  [0]=>
  string(8) "internal"
  [1]=>
  string(4) "user"
}
array(3) {
  [0]=>
  string(8) "gdfalpha"
  [1]=>
  string(7) "gdfbeta"
  [2]=>
  string(9) "gdf_gamma"
}
array(0) {
}
bool(true)
bool(true)
strlen     internal=1 user=0
scandir    internal=1 user=0
glob       internal=1 user=0
checkdate  internal=1 user=0
hex2bin    internal=1 user=0
class_uses internal=1 user=0
preg_grep  internal=1 user=0
array(1) {
  [0]=>
  string(8) "gdfdelta"
}
bool(true)
