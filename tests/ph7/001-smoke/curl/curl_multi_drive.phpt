--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The verbs that drive a multi handle, with nothing to transfer
--DESCRIPTION--
curl_multi_exec(), curl_multi_select(), curl_multi_info_read() and
curl_multi_getcontent() asked everything that does not need a peer. The
transfers themselves live in 002-integration, which has a server to talk to.

What only the empty set shows:

  * exec on a set with nothing in it is CURLM_OK with a still-running count of
    0, and it WRITES that count -- through a reference php declares untyped, so
    whatever the caller had there is replaced whatever its type was;

  * select answers 0 immediately rather than sleeping out its timeout: the wait
    is libcurl's, and it does not wait on a set with no socket. The bound on the
    timeout is php's own, since the seconds become an int of MILLISECONDS -- so
    2147483.647 is the last legal one and NAN is refused by the same comparison;

  * info_read on an empty queue answers false and does NOT touch its second
    argument, so a caller's variable survives a read that found nothing;

  * getcontent reads the DESTINATION that stands now: a handle collecting its
    body answers the empty string before any transfer, and one printing it, one
    calling back and one that was reset all answer null.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$mh = curl_multi_init();

echo "== exec on an empty set ==\n";
$still = 'not an int';
var_dump(curl_multi_exec($mh, $still), $still);
$still = null;
var_dump(curl_multi_exec($mh, $still), $still);

echo "== select on an empty set ==\n";
$t = microtime(true);
var_dump(curl_multi_select($mh, 0.25));
printf("waited less than the timeout: %s\n", var_export(microtime(true) - $t < 0.2, true));
var_dump(curl_multi_select($mh), curl_multi_select($mh, 0.0), curl_multi_select($mh, -0.0));

echo "== the timeout bound ==\n";
foreach ([2147483.647, 2147483.0, 1.0e-9, 2147483.6470001, 2147483.648, -1.0e-9, INF, -INF, NAN] as $v) {
    try { printf("%-16s => %d\n", var_export($v, true), curl_multi_select($mh, $v)); }
    catch (Throwable $e) { printf("%-16s => %s: %s\n", var_export($v, true), get_class($e), $e->getMessage()); }
}
// a string that is a number is one, and one that is not is a TypeError
try { var_dump(curl_multi_select($mh, '0.5')); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { var_dump(curl_multi_select($mh, 'x')); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "== info_read on an empty queue ==\n";
$queued = 'untouched';
var_dump(curl_multi_info_read($mh, $queued), $queued);
var_dump(curl_multi_info_read($mh));

echo "== getcontent reads the destination ==\n";
$plain = curl_init();
$ret = curl_init();
curl_setopt($ret, CURLOPT_RETURNTRANSFER, true);
$cb = curl_init();
curl_setopt($cb, CURLOPT_WRITEFUNCTION, function ($h, $c) { return strlen($c); });
$reset = curl_init();
curl_setopt($reset, CURLOPT_RETURNTRANSFER, true);
curl_reset($reset);
var_dump(curl_multi_getcontent($plain), curl_multi_getcontent($ret),
    curl_multi_getcontent($cb), curl_multi_getcontent($reset));
// asking twice answers twice
var_dump(curl_multi_getcontent($ret) === curl_multi_getcontent($ret));
// a handle in a set is still an ordinary handle to this verb
curl_multi_add_handle($mh, $ret);
var_dump(curl_multi_getcontent($ret));

echo "== the arguments ==\n";
foreach ([null, 1, 'x', $plain] as $v) {
    try { curl_multi_exec($v, $still); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach ([null, 1, 'x', $mh] as $v) {
    try { curl_multi_getcontent($v); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
try { curl_multi_exec($mh); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_multi_select(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_multi_info_read($mh, $queued, 1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "== none of them touches the error state ==\n";
$m2 = curl_multi_init();
$h = curl_init();
curl_multi_add_handle($m2, $h);
curl_multi_add_handle($m2, $h);
printf("after the refused add=%d\n", curl_multi_errno($m2));
curl_multi_select($m2, 0.0);
printf("after select=%d\n", curl_multi_errno($m2));
$q = null;
curl_multi_info_read($m2, $q);
printf("after info_read=%d\n", curl_multi_errno($m2));
curl_multi_getcontent($h);
printf("after getcontent=%d\n", curl_multi_errno($m2));
$s = null;
curl_multi_exec($m2, $s);
printf("after exec=%d\n", curl_multi_errno($m2));
--EXPECT--
== exec on an empty set ==
int(0)
int(0)
int(0)
int(0)
== select on an empty set ==
int(0)
waited less than the timeout: true
int(0)
int(0)
int(0)
== the timeout bound ==
2147483.647      => 0
2147483.0        => 0
1.0E-9           => 0
2147483.6470001  => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
2147483.648      => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
-1.0E-9          => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
INF              => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
-INF             => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
NAN              => ValueError: curl_multi_select(): Argument #2 ($timeout) must be between 0 and 2147483.647000
int(0)
TypeError: curl_multi_select(): Argument #2 ($timeout) must be of type float, string given
== info_read on an empty queue ==
bool(false)
string(9) "untouched"
bool(false)
== getcontent reads the destination ==
NULL
string(0) ""
NULL
NULL
bool(true)
string(0) ""
== the arguments ==
TypeError: curl_multi_exec(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, null given
TypeError: curl_multi_exec(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, int given
TypeError: curl_multi_exec(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, string given
TypeError: curl_multi_exec(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, CurlHandle given
TypeError: curl_multi_getcontent(): Argument #1 ($handle) must be of type CurlHandle, null given
TypeError: curl_multi_getcontent(): Argument #1 ($handle) must be of type CurlHandle, int given
TypeError: curl_multi_getcontent(): Argument #1 ($handle) must be of type CurlHandle, string given
TypeError: curl_multi_getcontent(): Argument #1 ($handle) must be of type CurlHandle, CurlMultiHandle given
ArgumentCountError: curl_multi_exec() expects exactly 2 arguments, 1 given
ArgumentCountError: curl_multi_select() expects at least 1 argument, 0 given
ArgumentCountError: curl_multi_info_read() expects at most 2 arguments, 3 given
== none of them touches the error state ==
after the refused add=7
after select=7
after info_read=7
after getcontent=7
after exec=0
