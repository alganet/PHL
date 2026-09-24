--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
set_error_handler()'s $error_levels decides which errors reach the handler at all, a `null` callback is a stack entry rather than a failure, and anything else uncallable is a TypeError
--FILE--
<?php
// The mask is the second argument, and a level outside it never reaches the
// handler -- it falls straight through to the engine's own reporting, which is
// why the notice and the deprecation below are printed rather than collected.
$sehl_seen = [];
set_error_handler(static function ($sehl_n, $sehl_m) use (&$sehl_seen) {
    $sehl_seen[] = "$sehl_n:$sehl_m";
    return true;
}, E_USER_WARNING);
trigger_error('w', E_USER_WARNING);
@trigger_error('n', E_USER_NOTICE);
@trigger_error('d', E_USER_DEPRECATED);
var_dump($sehl_seen);
restore_error_handler();

// A mask of 0 means the handler never runs.
$sehl_seen2 = [];
set_error_handler(static function ($sehl_n) use (&$sehl_seen2) { $sehl_seen2[] = $sehl_n; return true; }, 0);
@trigger_error('q', E_USER_WARNING);
var_dump($sehl_seen2);
restore_error_handler();

// The mask rides WITH the handler: a level the inner one refuses does NOT walk
// down to the outer handler, and restore_error_handler() pops both together.
$sehl_log = [];
set_error_handler(static function ($sehl_n) use (&$sehl_log) { $sehl_log[] = "outer$sehl_n"; return true; }, E_ALL);
set_error_handler(static function ($sehl_n) use (&$sehl_log) { $sehl_log[] = "inner$sehl_n"; return true; }, E_USER_ERROR);
@trigger_error('a', E_USER_WARNING);
restore_error_handler();
trigger_error('b', E_USER_WARNING);
var_dump($sehl_log);
restore_error_handler();

// Engine diagnostics are masked by the same bits.
$sehl_seen3 = [];
set_error_handler(static function ($sehl_n, $sehl_m) use (&$sehl_seen3) { $sehl_seen3[] = "$sehl_n/$sehl_m"; return true; }, E_NOTICE);
@$sehl_undefined_one;
var_dump($sehl_seen3);
restore_error_handler();
$sehl_seen4 = [];
set_error_handler(static function ($sehl_n, $sehl_m) use (&$sehl_seen4) { $sehl_seen4[] = "$sehl_n/$sehl_m"; return true; }, E_WARNING);
@$sehl_undefined_two;
var_dump($sehl_seen4);
restore_error_handler();

// It is a plain bitwise AND at full width, so 2^32 + E_USER_NOTICE still selects
// E_USER_NOTICE, and -1 selects everything.
foreach ([-1, PHP_INT_MAX, 4294967296 + 1024] as $sehl_mask) {
    $sehl_hit = [];
    set_error_handler(static function ($sehl_n) use (&$sehl_hit) { $sehl_hit[] = $sehl_n; return true; }, $sehl_mask);
    trigger_error('z', E_USER_NOTICE);
    restore_error_handler();
    var_dump($sehl_hit);
}

// The default is every level.
$sehl_seen5 = [];
set_error_handler(static function ($sehl_n) use (&$sehl_seen5) { $sehl_seen5[] = $sehl_n; return true; });
trigger_error('x', E_USER_DEPRECATED);
var_dump($sehl_seen5);
restore_error_handler();

// set_error_handler() answers the handler it REPLACES, and `null` is a real
// stack entry: it silences the handler until a restore brings the previous one
// -- with its own levels -- back.
$sehl_start = get_error_handler();
$sehl_a = static function ($sehl_n) { echo "A$sehl_n\n"; return true; };
var_dump(set_error_handler($sehl_a) === $sehl_start);
var_dump(set_error_handler(null) === $sehl_a);
@trigger_error('silent', E_USER_NOTICE);
var_dump(restore_error_handler());
trigger_error('back', E_USER_NOTICE);
var_dump(restore_error_handler());

set_error_handler($sehl_a, E_USER_WARNING);
set_error_handler(null, E_USER_NOTICE);
restore_error_handler();
trigger_error('w2', E_USER_WARNING);
@trigger_error('n2', E_USER_NOTICE);
restore_error_handler();

// Anything else that cannot be called is refused, naming why.
try { set_error_handler('sehl_no_such_function'); } catch (Throwable $sehl_e) { echo get_class($sehl_e), ': ', $sehl_e->getMessage(), "\n"; }
try { set_error_handler(42); } catch (Throwable $sehl_e) { echo get_class($sehl_e), ': ', $sehl_e->getMessage(), "\n"; }
// A refused argument installs nothing, so the stack is exactly where it started.
var_dump(get_error_handler() === $sehl_start);
?>
--EXPECT--
array(1) {
  [0]=>
  string(5) "512:w"
}
array(0) {
}
array(1) {
  [0]=>
  string(8) "outer512"
}
array(0) {
}
array(1) {
  [0]=>
  string(40) "2/Undefined variable $sehl_undefined_two"
}
array(1) {
  [0]=>
  int(1024)
}
array(1) {
  [0]=>
  int(1024)
}
array(1) {
  [0]=>
  int(1024)
}
array(1) {
  [0]=>
  int(16384)
}
bool(true)
bool(true)
bool(true)
A1024
bool(true)
A512
TypeError: set_error_handler(): Argument #1 ($callback) must be a valid callback or null, function "sehl_no_such_function" not found or invalid function name
TypeError: set_error_handler(): Argument #1 ($callback) must be a valid callback or null, no array or string given
bool(true)
