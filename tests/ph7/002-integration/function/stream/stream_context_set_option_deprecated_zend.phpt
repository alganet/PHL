--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: the two-argument stream_context_set_option() is deprecated and works (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php 8.3 keeps the array form working and emits E_DEPRECATED for it; PHL's
 * half of this pair refuses it outright (§10). The handler is here because a
 * php.ini that masks E_DEPRECATED would otherwise hide the whole point. */
set_error_handler(function ($n, $s) { echo "[", $n, "] ", $s, "\n"; return true; },
    E_ALL | E_DEPRECATED);
$c = stream_context_create();
var_dump(stream_context_set_option($c, ['http' => ['m' => 1]]));
var_dump(stream_context_get_options($c));
?>
--EXPECT--
[8192] Calling stream_context_set_option() with 2 arguments is deprecated, use stream_context_set_options() instead
bool(true)
array(1) {
  ["http"]=>
  array(1) {
    ["m"]=>
    int(1)
  }
}
