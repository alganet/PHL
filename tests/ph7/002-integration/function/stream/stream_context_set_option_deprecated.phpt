--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: the two-argument stream_context_set_option() is REFUSED (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* RECORDED POLICY DIVERGENCE (§10). php 8.3 DEPRECATES the two-argument
 * spelling — stream_context_set_option($ctx, $options_array) — in favour of
 * stream_context_set_options(). §10 removes what php merely deprecates, so the
 * array form is not a form here at all: argument #2 is a string, and a call
 * that omits the option name and the value is short by two arguments. */
$c = stream_context_create();
try { stream_context_set_option($c, ['http' => ['m' => 1]]); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { stream_context_set_option($c, ['http' => ['m' => 1]], 'x', 1); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { stream_context_set_option($c, 'http', 'm'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* The spelling that carries the whole array, and the one php's deprecation
 * points at. */
var_dump(stream_context_set_options($c, ['http' => ['m' => 1]]));
var_dump(stream_context_get_options($c));
?>
--EXPECT--
ArgumentCountError: stream_context_set_option() expects exactly 4 arguments, 2 given
TypeError: stream_context_set_option(): Argument #2 ($wrapper_name) must be of type string, array given
ArgumentCountError: stream_context_set_option() expects exactly 4 arguments, 3 given
bool(true)
array(1) {
  ["http"]=>
  array(1) {
    ["m"]=>
    int(1)
  }
}
