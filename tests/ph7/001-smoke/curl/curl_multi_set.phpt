--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_multi_init() and the SET of easy handles a multi holds
--DESCRIPTION--
The container half of php's multi interface: the handle class, the set it
holds, and the option setter. Everything here is about the set rather than
about any transfer, so nothing below talks to a socket.

Four answers are php's rather than libcurl's:

  * the set holds the OBJECTS. curl_multi_get_handles() answers the same
    instances that were added, in add order, re-indexed from 0 -- and the set
    keeps them alive, so `unset($h)` after an add is not the end of anything.
    A handle removed and added again is LAST.

  * curl_multi_close() EMPTIES the set instead of freeing it. Unlike
    curl_close(), which is a pure no-op, this one drops every handle -- and the
    multi still works afterwards, which is how the two halves are told apart.

  * the error state is per-VERB. add/remove/setopt write it; get_handles and
    the reporters leave it alone. Adding a handle the set already holds, and
    adding one another set holds, are the same code (CURLM_ADDED_ALREADY), and
    a refused OPTION leaves CURLM_UNKNOWN_OPTION (6) behind the ValueError --
    a code php declares no constant for.

  * CURLMOPT_PUSHFUNCTION is the only option that takes a callable, and null is
    NOT a way to clear it: where curl_setopt() reads null as "put the default
    back", this setter refuses it like any other non-callable. Its diagnostics
    also blame argument #2 ($option) -- the argument the value did not come
    from -- which is php's own wording.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$mh = curl_multi_init();
var_dump(get_class($mh), $mh instanceof CurlMultiHandle, is_object($mh), is_resource($mh));

// php presents NOTHING on a multi handle: no property on any surface.
print_r($mh);
echo "\n";
var_export($mh);
echo "\n";
var_dump((array) $mh, get_object_vars($mh), json_encode($mh));

$rc = new ReflectionClass('CurlMultiHandle');
var_dump($rc->isFinal(), $rc->isInstantiable(), count($rc->getMethods()),
    count($rc->getProperties()), count($rc->getConstants()));

echo "== the refusals ==\n";
foreach ([fn() => new CurlMultiHandle(), fn() => clone $mh, fn() => serialize($mh)] as $f) {
    try { $f(); echo "no throw\n"; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "== the set ==\n";
$a = curl_init();
$b = curl_init();
$c = curl_init();
printf("add a=%d, add a again=%d, add b=%d, add c=%d\n", curl_multi_add_handle($mh, $a),
    curl_multi_add_handle($mh, $a), curl_multi_add_handle($mh, $b), curl_multi_add_handle($mh, $c));
$name = fn($x) => $x === $a ? 'a' : ($x === $b ? 'b' : ($x === $c ? 'c' : '?'));
$show = function ($mh) use ($name) {
    $h = curl_multi_get_handles($mh);
    printf("  [%s] keys=%s\n", implode(',', array_map($name, $h)), implode(',', array_keys($h)));
};
$show($mh);
printf("remove b=%d, remove b again=%d\n", curl_multi_remove_handle($mh, $b),
    curl_multi_remove_handle($mh, $b));
$show($mh);
// re-added goes to the end
curl_multi_add_handle($mh, $b);
$show($mh);

// a handle another set holds is CURLM_ADDED_ALREADY there too, and removing it
// from the set that does NOT hold it is libcurl's CURLM_BAD_EASY_HANDLE
$m2 = curl_multi_init();
printf("add a elsewhere=%d, remove a from elsewhere=%d, its handles=%d\n",
    curl_multi_add_handle($m2, $a), curl_multi_remove_handle($m2, $a),
    count(curl_multi_get_handles($m2)));

echo "== the set keeps the object alive ==\n";
$m3 = curl_multi_init();
$tmp = curl_init();
$id = spl_object_id($tmp);
curl_multi_add_handle($m3, $tmp);
unset($tmp);
$held = curl_multi_get_handles($m3);
var_dump(count($held), spl_object_id($held[0]) === $id, get_class($held[0]));

echo "== the error state ==\n";
$m4 = curl_multi_init();
$x = curl_init();
printf("fresh=%d\n", curl_multi_errno($m4));
curl_multi_add_handle($m4, $x);
printf("after an add=%d\n", curl_multi_errno($m4));
curl_multi_add_handle($m4, $x);
printf("after the same add=%d\n", curl_multi_errno($m4));
curl_multi_get_handles($m4);
printf("after get_handles=%d\n", curl_multi_errno($m4));
curl_multi_remove_handle($m4, $x);
printf("after a remove=%d\n", curl_multi_errno($m4));
curl_multi_add_handle($m4, $x);
curl_multi_add_handle($m4, $x);
try { curl_multi_setopt($m4, 999999, 1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
printf("after a refused option=%d\n", curl_multi_errno($m4));
curl_multi_setopt($m4, CURLMOPT_MAXCONNECTS, 4);
printf("after a good option=%d\n", curl_multi_errno($m4));

echo "== close empties the set, and the set goes on working ==\n";
$m5 = curl_multi_init();
$y = curl_init();
curl_multi_add_handle($m5, $y);
var_dump(curl_multi_close($m5));
printf("errno=%d handles=%d add after the close=%d handles=%d\n", curl_multi_errno($m5),
    count(curl_multi_get_handles($m5)), curl_multi_add_handle($m5, curl_init()),
    count(curl_multi_get_handles($m5)));
var_dump(curl_multi_close($m5));

echo "== the options ==\n";
$m6 = curl_multi_init();
foreach (['CURLMOPT_PIPELINING' => 2, 'CURLMOPT_MAXCONNECTS' => 5,
          'CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE' => 100, 'CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE' => 100,
          'CURLMOPT_MAX_HOST_CONNECTIONS' => 2, 'CURLMOPT_MAX_PIPELINE_LENGTH' => 3,
          'CURLMOPT_MAX_TOTAL_CONNECTIONS' => 4, 'CURLMOPT_MAX_CONCURRENT_STREAMS' => 100] as $n => $v) {
    printf("%-36s => %s errno=%d\n", $n, var_export(curl_multi_setopt($m6, constant($n), $v), true),
        curl_multi_errno($m6));
}
// every non-callable option takes the ordinary int cast, refusing nothing
foreach ([['abc'], [[1]], [null], [true], [1.9], ['7']] as $v) {
    printf("  %-10s => %s\n", str_replace("\n", '', var_export($v[0], true)),
        var_export(curl_multi_setopt($m6, CURLMOPT_MAXCONNECTS, $v[0]), true));
}
// CURLPIPE_HTTP1 -- which is what a plain `true` casts to -- is gone from
// libcurl, and php says so in a warning of its own before answering true
set_error_handler(function ($no, $msg) { printf("  [%d] %s\n", $no, $msg); return true; });
var_dump(curl_multi_setopt($m6, CURLMOPT_PIPELINING, true));
var_dump(curl_multi_setopt($m6, CURLMOPT_PIPELINING, CURLPIPE_NOTHING));
restore_error_handler();

echo "== the push callback ==\n";
var_dump(curl_multi_setopt($m6, CURLMOPT_PUSHFUNCTION, function ($p, $c, $h) { return CURL_PUSH_DENY; }));
var_dump(curl_multi_setopt($m6, CURLMOPT_PUSHFUNCTION, 'curl_multi_set_push'));
foreach ([null, 'nosuchfn', 5, [1], [1, 2, 3], new stdClass] as $v) {
    try { var_dump(curl_multi_setopt($m6, CURLMOPT_PUSHFUNCTION, $v)); }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "== the arguments ==\n";
foreach ([null, 1, 'x', curl_init()] as $v) {
    try { curl_multi_add_handle($v, curl_init()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach ([null, 1, 'x', $mh] as $v) {
    try { curl_multi_add_handle($mh, $v); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
try { curl_multi_init(1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_multi_errno(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

function curl_multi_set_push($parent, $child, $headers)
{
    return CURL_PUSH_DENY;
}
--EXPECT--
string(15) "CurlMultiHandle"
bool(true)
bool(true)
bool(false)
CurlMultiHandle Object
(
)

\CurlMultiHandle::__set_state(array(
))
array(0) {
}
array(0) {
}
string(2) "{}"
bool(true)
bool(true)
int(0)
int(0)
int(0)
== the refusals ==
Error: Cannot directly construct CurlMultiHandle, use curl_multi_init() instead
Error: Trying to clone an uncloneable object of class CurlMultiHandle
Exception: Serialization of 'CurlMultiHandle' is not allowed
== the set ==
add a=0, add a again=7, add b=0, add c=0
  [a,b,c] keys=0,1,2
remove b=0, remove b again=0
  [a,c] keys=0,1
  [a,c,b] keys=0,1,2
add a elsewhere=7, remove a from elsewhere=2, its handles=0
== the set keeps the object alive ==
int(1)
bool(true)
string(10) "CurlHandle"
== the error state ==
fresh=0
after an add=0
after the same add=7
after get_handles=7
after a remove=0
ValueError: curl_multi_setopt(): Argument #2 ($option) is not a valid cURL multi option
after a refused option=6
after a good option=0
== close empties the set, and the set goes on working ==
NULL
errno=0 handles=0 add after the close=0 handles=1
NULL
== the options ==
CURLMOPT_PIPELINING                  => true errno=0
CURLMOPT_MAXCONNECTS                 => true errno=0
CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE   => true errno=0
CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE => true errno=0
CURLMOPT_MAX_HOST_CONNECTIONS        => true errno=0
CURLMOPT_MAX_PIPELINE_LENGTH         => true errno=0
CURLMOPT_MAX_TOTAL_CONNECTIONS       => true errno=0
CURLMOPT_MAX_CONCURRENT_STREAMS      => true errno=0
  'abc'      => true
  array (  0 => 1,) => true
  NULL       => true
  true       => true
  1.9        => true
  '7'        => true
  [2] curl_multi_setopt(): CURLPIPE_HTTP1 is no longer supported
bool(true)
bool(true)
== the push callback ==
bool(true)
bool(true)
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, no array or string given
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, function "nosuchfn" not found or invalid function name
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, no array or string given
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, array callback must have exactly two members
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, array callback must have exactly two members
TypeError: curl_multi_setopt(): Argument #2 ($option) must be a valid callback for option CURLMOPT_PUSHFUNCTION, no array or string given
== the arguments ==
TypeError: curl_multi_add_handle(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, null given
TypeError: curl_multi_add_handle(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, int given
TypeError: curl_multi_add_handle(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, string given
TypeError: curl_multi_add_handle(): Argument #1 ($multi_handle) must be of type CurlMultiHandle, CurlHandle given
TypeError: curl_multi_add_handle(): Argument #2 ($handle) must be of type CurlHandle, null given
TypeError: curl_multi_add_handle(): Argument #2 ($handle) must be of type CurlHandle, int given
TypeError: curl_multi_add_handle(): Argument #2 ($handle) must be of type CurlHandle, string given
TypeError: curl_multi_add_handle(): Argument #2 ($handle) must be of type CurlHandle, CurlMultiHandle given
ArgumentCountError: curl_multi_init() expects exactly 0 arguments, 1 given
ArgumentCountError: curl_multi_errno() expects exactly 1 argument, 0 given
