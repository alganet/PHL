--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the error and exception handler stacks have no depth limit: each restore brings back exactly what the matching set replaced, levels included
--FILE--
<?php
// Three deep. Each restore has to find the handler its own set displaced, so
// nothing under the top entry may be overwritten.
$hsd_log = [];
$hsd_a = static function ($hsd_n) use (&$hsd_log) { $hsd_log[] = "A$hsd_n"; return true; };
$hsd_b = static function ($hsd_n) use (&$hsd_log) { $hsd_log[] = "B$hsd_n"; return true; };
$hsd_c = static function ($hsd_n) use (&$hsd_log) { $hsd_log[] = "C$hsd_n"; return true; };
$hsd_start = get_error_handler();

set_error_handler($hsd_a, E_USER_NOTICE);
set_error_handler($hsd_b, E_USER_WARNING);
set_error_handler($hsd_c, E_USER_DEPRECATED);
@trigger_error('1', E_USER_DEPRECATED);
restore_error_handler();
@trigger_error('2', E_USER_WARNING);
restore_error_handler();
@trigger_error('3', E_USER_NOTICE);
restore_error_handler();
var_dump($hsd_log);
var_dump(get_error_handler() === $hsd_start);

// Each level's own $error_levels came back with it: the depths above only ever
// answered the ONE level they were registered for.
$hsd_log = [];
set_error_handler($hsd_a, E_USER_NOTICE);
set_error_handler($hsd_b, E_USER_WARNING);
@trigger_error('x', E_USER_NOTICE);    /* B's mask misses, and A is NOT reached */
@trigger_error('x', E_USER_WARNING);   /* B */
restore_error_handler();
@trigger_error('y', E_USER_NOTICE);    /* A */
@trigger_error('y', E_USER_WARNING);   /* A's mask misses */
restore_error_handler();
var_dump($hsd_log);

// Ten deep, with a `null` reset buried in the middle of it.
$hsd_seen = [];
for ($hsd_i = 0; $hsd_i < 10; $hsd_i++) {
    if ($hsd_i === 5) {
        set_error_handler(null);
        continue;
    }
    set_error_handler(static function ($hsd_n) use ($hsd_i, &$hsd_seen) { $hsd_seen[] = $hsd_i; return true; });
}
for ($hsd_i = 0; $hsd_i < 10; $hsd_i++) {
    @trigger_error('t', E_USER_NOTICE);
    restore_error_handler();
}
var_dump($hsd_seen);
var_dump(get_error_handler() === $hsd_start);

// The exception handler is the same stack, and set_exception_handler() answers
// the handler it replaces too.
$hsd_x = static function ($hsd_e) { echo 'X:', $hsd_e->getMessage(), "\n"; };
$hsd_y = static function ($hsd_e) { echo 'Y:', $hsd_e->getMessage(), "\n"; };
$hsd_estart = get_exception_handler();
var_dump(set_exception_handler($hsd_x) === $hsd_estart);
var_dump(set_exception_handler($hsd_y) === $hsd_x);
var_dump(set_exception_handler(null) === $hsd_y);
var_dump(restore_exception_handler());
var_dump(get_exception_handler() === $hsd_y);
restore_exception_handler();
var_dump(get_exception_handler() === $hsd_x);
restore_exception_handler();
var_dump(get_exception_handler() === $hsd_estart);

// A restore always answers TRUE -- the return value says "the call is valid",
// not "a handler was in place".
set_exception_handler(null);
var_dump(get_exception_handler(), restore_exception_handler());

// A refused handler installs nothing, so the stack does not move.
try { set_exception_handler('hsd_no_such_function'); } catch (Throwable $hsd_e) { echo get_class($hsd_e), ': ', $hsd_e->getMessage(), "\n"; }
var_dump(get_exception_handler() === $hsd_estart);

// A handler is HIDDEN for the duration of its own call: a diagnostic it raises
// itself does not re-enter it, and get_error_handler() reports none.
$hsd_depth = 0;
set_error_handler(static function ($hsd_n, $hsd_m) use (&$hsd_depth) {
    $hsd_depth++;
    echo 'H:', $hsd_m, ' inside=', var_export(get_error_handler(), true), "\n";
    @trigger_error('again', E_USER_NOTICE);
    return true;
});
@trigger_error('top', E_USER_NOTICE);
echo 'depth=', $hsd_depth, ' after=', var_export(get_error_handler() !== null, true), "\n";
restore_error_handler();

// So a set_error_handler() from INSIDE one replaces that empty entry, and what
// the handler leaves behind is what stays installed.
$hsd_log = [];
set_error_handler(static function ($hsd_n, $hsd_m) use (&$hsd_log) {
    $hsd_log[] = "outer:$hsd_m";
    set_error_handler(static function ($hsd_n2, $hsd_m2) use (&$hsd_log) { $hsd_log[] = "inner:$hsd_m2"; return true; });
    return true;
});
@trigger_error('one', E_USER_NOTICE);
@trigger_error('two', E_USER_NOTICE);
restore_error_handler();
@trigger_error('three', E_USER_NOTICE);
// The inner handler's own entry is what the restore pops, so nothing is left.
var_dump($hsd_log, get_error_handler());

// An UNTOUCHED slot gets the original back -- including after the handler resets
// it with null, which leaves the slot empty just like never touching it.
$hsd_log = [];
set_error_handler(static function ($hsd_n, $hsd_m) use (&$hsd_log) {
    $hsd_log[] = "A:$hsd_m";
    if ($hsd_m === '1') { set_error_handler(null); }
    return true;
});
@trigger_error('1', E_USER_NOTICE);
@trigger_error('2', E_USER_NOTICE);
var_dump($hsd_log);
// Every entry those two blocks pushed is still there to be popped, the ones the
// handlers pushed from inside themselves included, and the bottom of the stack
// is exactly what this test found installed.
for ($hsd_i = 0; $hsd_i < 10 && get_error_handler() !== $hsd_start; $hsd_i++) {
    restore_error_handler();
}
var_dump(get_error_handler() === $hsd_start);
?>
--EXPECT--
array(3) {
  [0]=>
  string(6) "C16384"
  [1]=>
  string(4) "B512"
  [2]=>
  string(5) "A1024"
}
bool(true)
array(2) {
  [0]=>
  string(4) "B512"
  [1]=>
  string(5) "A1024"
}
array(9) {
  [0]=>
  int(9)
  [1]=>
  int(8)
  [2]=>
  int(7)
  [3]=>
  int(6)
  [4]=>
  int(4)
  [5]=>
  int(3)
  [6]=>
  int(2)
  [7]=>
  int(1)
  [8]=>
  int(0)
}
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
NULL
bool(true)
TypeError: set_exception_handler(): Argument #1 ($callback) must be a valid callback or null, function "hsd_no_such_function" not found or invalid function name
bool(true)
H:top inside=NULL
depth=1 after=true
array(2) {
  [0]=>
  string(9) "outer:one"
  [1]=>
  string(9) "inner:two"
}
NULL
array(2) {
  [0]=>
  string(3) "A:1"
  [1]=>
  string(3) "A:2"
}
bool(true)
