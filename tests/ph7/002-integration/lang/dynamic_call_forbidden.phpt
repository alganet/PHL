--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The six functions that read their caller's frame refuse a dynamic call
--DESCRIPTION--
compact, extract, get_defined_vars, func_get_args, func_num_args and func_get_arg answer
from the frame of whoever CALLED them. A call through a computed name (`$n()`), a Closure
(`compact(...)`, Closure::fromCallable, Reflection's getClosure), an internal function
driving a callback (array_map, usort, array_walk, ReflectionFunction::invoke) or a
call_user_func whose callable is not a literal puts an internal frame where that caller
should be, and php's zend_forbid_dynamic_call() throws `Cannot call f() dynamically`.
PHL ran every one of those and answered from the wrong frame.

The refusal is a RUNTIME check inside each body, placed where php places it: after the
function's own argument screens. `$n = 'compact'; $n()` is an ArgumentCountError, a bad
extract flag is its ValueError, a negative func_get_arg position is its ValueError, and
only then the call shape is looked at. What is NOT dynamic is what php's compiler folds
into a direct call: a literal name, `('compact')('a')`, and `call_user_func('compact', 'a')`
with a literal callable, which all still answer.
--FILE--
<?php
/* php's zend_forbid_dynamic_call(): six functions answer from their CALLER's frame,
 * and refuse a call that puts an internal frame where that caller should be. */
function t($label, $f) {
  try { $r = $f(); echo "$label: ", is_object($r) ? get_class($r) : json_encode($r), "\n"; }
  catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
function doors($n) {
  $a = 1; $b = 2;
  foreach ([
    'var' => fn() => $n(),
    'call_user_func' => fn() => call_user_func($n),
    'call_user_func_array' => fn() => call_user_func_array($n, []),
    'array_map' => fn() => array_map($n, [1]),
    'fromCallable' => fn() => Closure::fromCallable($n),
    'fcc' => fn() => $n(...),
    'usort' => function() use ($n) { $x = [2, 1]; usort($x, $n); return $x; },
  ] as $door => $f) {
    t("$n/$door", $f);
  }
}
foreach (['compact', 'extract', 'get_defined_vars', 'func_get_args', 'func_num_args', 'func_get_arg'] as $n) {
  doors($n);
}
echo "\n";
function f($p, $q = 2) {
  $a = 1;
  t('literal compact', function() use ($a) { return compact('a'); });
  t('literal COMPACT', function() use ($a) { return COMPACT('a'); });
  t('qualified literal compact', function() use ($a) { return \compact('a'); });
  t('parenthesized literal compact', function() use ($a) { return ('compact')('a'); });
  t('folded call_user_func compact', function() use ($a) { return call_user_func('compact', 'a'); });
  t('folded call_user_func_array compact', function() use ($a) { return call_user_func_array('compact', ['a']); });
  t('folded call_user_func extract', function() { return call_user_func('extract', ['z' => 1]); });
  t('folded call_user_func func_num_args', fn() => call_user_func('func_num_args'));
  t('literal func_get_args', fn() => func_get_args());
  t('fcc compact', function() use ($a) { $c = compact(...); return $c('a'); });
  t('fcc compact temporary', function() use ($a) { return (compact(...))('a'); });
  t('fromCallable compact', function() use ($a) { $c = Closure::fromCallable('compact'); return $c('a'); });
  t('reflection invoke compact', function() use ($a) { return (new ReflectionFunction('compact'))->invoke('a'); });
  t('reflection invokeArgs compact', function() use ($a) { return (new ReflectionFunction('compact'))->invokeArgs(['a']); });
  t('reflection getClosure compact', function() use ($a) { return (new ReflectionFunction('compact'))->getClosure()('a'); });
  t('var compact', function() use ($a) { $n = 'compact'; return $n('a'); });
  t('var COMPACT', function() use ($a) { $n = 'COMPACT'; return $n('a'); });
  t('var compact bad argument', function() use ($a) { $n = 'compact'; return $n(5); });
  t('var compact named argument', function() use ($a) { $n = 'compact'; return $n(var_name: 'a'); });
  t('var compact no argument', function() use ($a) { $n = 'compact'; return $n(); });
  t('var call_user_func compact', function() use ($a) { $n = 'compact'; return call_user_func($n, 'a'); });
  t('var call_user_func_array compact', function() use ($a) { $n = 'compact'; return call_user_func_array($n, ['a']); });
  t('var extract', function() { $n = 'extract'; return $n(['z' => 1]); });
  t('var extract bad flag', function() { $n = 'extract'; return $n(['z' => 1], 99); });
  t('var extract prefix missing', function() { $n = 'extract'; return $n(['z' => 1], EXTR_PREFIX_ALL); });
  t('var extract bad prefix', function() { $n = 'extract'; return $n(['z' => 1], EXTR_PREFIX_ALL, '1x'); });
  t('array_map extract', function() { return array_map('extract', [['z' => 1]]); });
  t('var func_get_args', fn() => (function($n) { return $n(); })('func_get_args'));
  t('var func_num_args', fn() => (function($n) { return $n(); })('func_num_args'));
  t('var func_num_args with argument', function() { $n = 'func_num_args'; return $n(1); });
  t('var func_get_arg negative', fn() => (function($n) { return $n(-1); })('func_get_arg'));
  t('var func_get_arg 0', fn() => (function($n) { return $n(0); })('func_get_arg'));
  t('var func_get_arg out of range', fn() => (function($n) { return $n(5); })('func_get_arg'));
  t('var func_get_arg string', fn() => (function($n) { return $n('x'); })('func_get_arg'));
  t('fcc func_get_args', function() { $c = func_get_args(...); return $c(); });
  t('fcc get_defined_vars', function() { $c = get_defined_vars(...); return $c(); });
  t('var get_defined_vars with argument', function() { $n = 'get_defined_vars'; return $n(1); });
  t('array_walk get_defined_vars', function() { $x = [1]; array_walk($x, 'get_defined_vars'); return $x; });
}
f(1);
echo "\n";
$g = 'get_defined_vars'; t('global var get_defined_vars', fn() => $g());
$h = 'func_get_args'; t('global var func_get_args', fn() => $h());
$h = 'func_num_args'; t('global var func_num_args', fn() => $h());
$h = 'func_get_arg'; t('global var func_get_arg', fn() => $h(0));
$h = 'compact'; $zz = 1; t('global var compact', fn() => $h('zz'));
$h = 'extract'; t('global var extract', fn() => $h(['zz' => 2]));
t('global call_user_func extract', fn() => call_user_func('extract', ['yy' => 3]));
var_dump(isset($yy), isset($zz) ? $zz : null);
--EXPECT--
compact/var: ArgumentCountError: compact() expects at least 1 argument, 0 given
compact/call_user_func: ArgumentCountError: compact() expects at least 1 argument, 0 given
compact/call_user_func_array: ArgumentCountError: compact() expects at least 1 argument, 0 given
compact/array_map: Error: Cannot call compact() dynamically
compact/fromCallable: Closure
compact/fcc: Closure
compact/usort: Error: Cannot call compact() dynamically
extract/var: ArgumentCountError: extract() expects at least 1 argument, 0 given
extract/call_user_func: ArgumentCountError: extract() expects at least 1 argument, 0 given
extract/call_user_func_array: ArgumentCountError: extract() expects at least 1 argument, 0 given
extract/array_map: TypeError: extract(): Argument #1 ($array) must be of type array, int given
extract/fromCallable: Closure
extract/fcc: Closure
extract/usort: TypeError: extract(): Argument #1 ($array) must be of type array, int given
get_defined_vars/var: Error: Cannot call get_defined_vars() dynamically
get_defined_vars/call_user_func: Error: Cannot call get_defined_vars() dynamically
get_defined_vars/call_user_func_array: Error: Cannot call get_defined_vars() dynamically
get_defined_vars/array_map: ArgumentCountError: get_defined_vars() expects exactly 0 arguments, 1 given
get_defined_vars/fromCallable: Closure
get_defined_vars/fcc: Closure
get_defined_vars/usort: ArgumentCountError: get_defined_vars() expects exactly 0 arguments, 2 given
func_get_args/var: Error: Cannot call func_get_args() dynamically
func_get_args/call_user_func: Error: Cannot call func_get_args() dynamically
func_get_args/call_user_func_array: Error: Cannot call func_get_args() dynamically
func_get_args/array_map: ArgumentCountError: func_get_args() expects exactly 0 arguments, 1 given
func_get_args/fromCallable: Closure
func_get_args/fcc: Closure
func_get_args/usort: ArgumentCountError: func_get_args() expects exactly 0 arguments, 2 given
func_num_args/var: Error: Cannot call func_num_args() dynamically
func_num_args/call_user_func: Error: Cannot call func_num_args() dynamically
func_num_args/call_user_func_array: Error: Cannot call func_num_args() dynamically
func_num_args/array_map: ArgumentCountError: func_num_args() expects exactly 0 arguments, 1 given
func_num_args/fromCallable: Closure
func_num_args/fcc: Closure
func_num_args/usort: ArgumentCountError: func_num_args() expects exactly 0 arguments, 2 given
func_get_arg/var: ArgumentCountError: func_get_arg() expects exactly 1 argument, 0 given
func_get_arg/call_user_func: ArgumentCountError: func_get_arg() expects exactly 1 argument, 0 given
func_get_arg/call_user_func_array: ArgumentCountError: func_get_arg() expects exactly 1 argument, 0 given
func_get_arg/array_map: Error: Cannot call func_get_arg() dynamically
func_get_arg/fromCallable: Closure
func_get_arg/fcc: Closure
func_get_arg/usort: ArgumentCountError: func_get_arg() expects exactly 1 argument, 2 given

literal compact: {"a":1}
literal COMPACT: {"a":1}
qualified literal compact: {"a":1}
parenthesized literal compact: {"a":1}
folded call_user_func compact: {"a":1}
folded call_user_func_array compact: {"a":1}
folded call_user_func extract: 1
folded call_user_func func_num_args: 0
literal func_get_args: []
fcc compact: Error: Cannot call compact() dynamically
fcc compact temporary: Error: Cannot call compact() dynamically
fromCallable compact: Error: Cannot call compact() dynamically
reflection invoke compact: Error: Cannot call compact() dynamically
reflection invokeArgs compact: Error: Cannot call compact() dynamically
reflection getClosure compact: Error: Cannot call compact() dynamically
var compact: Error: Cannot call compact() dynamically
var COMPACT: Error: Cannot call compact() dynamically
var compact bad argument: Error: Cannot call compact() dynamically
var compact named argument: Error: Cannot call compact() dynamically
var compact no argument: ArgumentCountError: compact() expects at least 1 argument, 0 given
var call_user_func compact: Error: Cannot call compact() dynamically
var call_user_func_array compact: Error: Cannot call compact() dynamically
var extract: Error: Cannot call extract() dynamically
var extract bad flag: ValueError: extract(): Argument #2 ($flags) must be a valid extract type
var extract prefix missing: ValueError: extract(): Argument #3 ($prefix) is required when using this extract type
var extract bad prefix: ValueError: extract(): Argument #3 ($prefix) must be a valid identifier
array_map extract: Error: Cannot call extract() dynamically
var func_get_args: Error: Cannot call func_get_args() dynamically
var func_num_args: Error: Cannot call func_num_args() dynamically
var func_num_args with argument: ArgumentCountError: func_num_args() expects exactly 0 arguments, 1 given
var func_get_arg negative: ValueError: func_get_arg(): Argument #1 ($position) must be greater than or equal to 0
var func_get_arg 0: Error: Cannot call func_get_arg() dynamically
var func_get_arg out of range: Error: Cannot call func_get_arg() dynamically
var func_get_arg string: TypeError: func_get_arg(): Argument #1 ($position) must be of type int, string given
fcc func_get_args: Error: Cannot call func_get_args() dynamically
fcc get_defined_vars: Error: Cannot call get_defined_vars() dynamically
var get_defined_vars with argument: ArgumentCountError: get_defined_vars() expects exactly 0 arguments, 1 given
array_walk get_defined_vars: ArgumentCountError: get_defined_vars() expects exactly 0 arguments, 2 given

global var get_defined_vars: Error: Cannot call get_defined_vars() dynamically
global var func_get_args: Error: Cannot call func_get_args() dynamically
global var func_num_args: Error: Cannot call func_num_args() dynamically
global var func_get_arg: Error: Cannot call func_get_arg() dynamically
global var compact: Error: Cannot call compact() dynamically
global var extract: Error: Cannot call extract() dynamically
global call_user_func extract: 1
bool(false)
int(1)
