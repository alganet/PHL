--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a stream context is released when its LAST value goes away
--FILE--
<?php
/* A dropped context is FREED, not held until the interpreter goes down: the
 * growth below is a bound rather than a number, because the two engines size
 * their allocations differently and both answer well under it. Twenty thousand
 * retained contexts is tens of megabytes. */
$clr_base = memory_get_usage(true);
for ($clr_i = 0; $clr_i < 20000; $clr_i++) {
    $clr_c = stream_context_create(['http' => ['method' => 'POST']]);
    $clr_c = null;
}
var_dump(memory_get_usage(true) - $clr_base < 4 * 1024 * 1024);

/* A discarded result is a dropped value too. */
$clr_base = memory_get_usage(true);
for ($clr_i = 0; $clr_i < 20000; $clr_i++) {
    stream_context_create(['http' => ['header' => 'X: 1']]);
}
var_dump(memory_get_usage(true) - $clr_base < 4 * 1024 * 1024);

/* A SECOND value keeps it alive, and both name one context. */
$clr_a = stream_context_create(['http' => ['method' => 'PUT']]);
$clr_b = $clr_a;
$clr_a = null;
var_dump(is_resource($clr_b), get_resource_type($clr_b));
var_dump(stream_context_get_options($clr_b));
stream_context_set_option($clr_b, 'http', 'timeout', 3);
var_dump(stream_context_get_options($clr_b)['http']['timeout']);

/* An array element and a by-value argument are values too. */
$clr_arr = ['c' => stream_context_create(['http' => ['method' => 'HEAD']])];
function clr_method($clr_p) { return stream_context_get_options($clr_p)['http']['method']; }
var_dump(clr_method($clr_arr['c']));
unset($clr_arr);

/* A handle that carries a context holds it past every value the script had. */
$clr_file = tempnam(sys_get_temp_dir(), 'clr');
file_put_contents($clr_file, "abc");
$clr_h = fopen($clr_file, 'r', false, stream_context_create(['http' => ['method' => 'GET']]));
var_dump(stream_context_get_options($clr_h));
var_dump(fread($clr_h, 3));
fclose($clr_h);

/* And the DEFAULT context outlives every value that names it. */
$clr_d = stream_context_get_default();
var_dump(get_resource_type($clr_d));
$clr_d = null;
var_dump(get_resource_type(stream_context_get_default()));

@unlink($clr_file);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(14) "stream-context"
array(1) {
  ["http"]=>
  array(1) {
    ["method"]=>
    string(3) "PUT"
  }
}
int(3)
string(4) "HEAD"
array(0) {
}
string(3) "abc"
string(14) "stream-context"
string(14) "stream-context"
--CLEAN--
<?php
unset($clr_base, $clr_i, $clr_c, $clr_a, $clr_b, $clr_arr, $clr_file, $clr_h, $clr_d);
