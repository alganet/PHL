--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A php callback runs from inside libcurl, and a throw out of one comes back from curl_exec()
--DESCRIPTION--
CURLOPT_WRITEFUNCTION and the progress pair, over file:// so no socket is
involved. The write callback is handed (handle, chunk) and must answer the
chunk's own LENGTH: any other answer -- 0, -1, true, a non-numeric string --
stops the transfer with CURLE_WRITE_ERROR. Setting one replaces the
destination entirely, so neither RETURNTRANSFER's buffer nor the script's
output sees the body.

The throw is the interesting one. libcurl is mid-transfer when the engine
re-enters PHP, and a throw cannot travel back through the library's C frames --
so it is PARKED on the handle, libcurl is stopped with a value it reads as a
failure, and curl_exec() raises the parked status once the library has
unwound. php's own answer is what makes that observable: the exception comes
out of curl_exec() unchanged AND curl_errno() is left at 0, not at the
CURLE_WRITE_ERROR the stopping return would otherwise have set.

php accepts every callable SHAPE for these options, and null puts the default
back. The three TypeError tails say which way a non-callable was wrong.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$tmp = tempnam(sys_get_temp_dir(), 'curlcb');
file_put_contents($tmp, str_repeat("line\n", 3));
$url = 'file://' . (DIRECTORY_SEPARATOR === '\\' ? '/' . str_replace('\\', '/', $tmp) : $tmp);

echo "== the write callback's arguments ==\n";
$h = curl_init($url);
curl_setopt($h, CURLOPT_WRITEFUNCTION, function (...$args) {
    printf("  argc=%d types=%s\n", count($args),
        implode(',', array_map(fn($a) => is_object($a) ? get_class($a) : gettype($a), $args)));
    printf("  chunk=%s\n", var_export($args[1], true));
    return strlen($args[1]);
});
var_dump(curl_exec($h));

echo "== the return value is a byte count ==\n";
foreach ([0, -1, 'x', null, true] as $ret) {
    $k = curl_init($url);
    curl_setopt($k, CURLOPT_WRITEFUNCTION, fn($ch, $d) => $ret);
    ob_start();
    $r = curl_exec($k);
    ob_end_clean();
    printf("  return %-6s => exec %s errno %d\n", var_export($ret, true), var_export($r, true), curl_errno($k));
}

echo "== a callback that throws ==\n";
$t = curl_init($url);
curl_setopt($t, CURLOPT_WRITEFUNCTION, function ($ch, $d) { throw new RuntimeException('from the callback'); });
try { echo '  exec => ', var_export(curl_exec($t), true), "\n"; }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
printf("  errno after the throw: %d\n", curl_errno($t));

echo "== the shapes setopt takes ==\n";
function curlCbNamed($ch, $d) { return strlen($d); }
class CurlCbHolder {
    public function m($ch, $d) { return strlen($d); }
    public static function sm($ch, $d) { return strlen($d); }
    public function __invoke($ch, $d) { return strlen($d); }
}
$o = new CurlCbHolder;
foreach ([
    'closure' => fn($ch, $d) => strlen($d),
    'string' => 'curlCbNamed',
    'array-obj' => [$o, 'm'],
    'array-static' => ['CurlCbHolder', 'sm'],
    'string-static' => 'CurlCbHolder::sm',
    'invokable' => $o,
    'null' => null,
    'missing-fn' => 'no_such_function_curlcb',
    'not-callable' => 42,
    'array-bad' => ['a', 'b', 'c'],
] as $label => $cb) {
    $c = curl_init($url);
    try { printf("  %-14s => %s\n", $label, var_export(curl_setopt($c, CURLOPT_WRITEFUNCTION, $cb), true)); }
    catch (Throwable $e) { printf("  %-14s => %s: %s\n", $label, get_class($e), $e->getMessage()); }
}

echo "== the progress callback ==\n";
$q = curl_init($url);
curl_setopt($q, CURLOPT_RETURNTRANSFER, true);
curl_setopt($q, CURLOPT_NOPROGRESS, false);
$shape = null;
curl_setopt($q, CURLOPT_XFERINFOFUNCTION, function (...$a) use (&$shape) {
    $shape ??= count($a) . ':' . implode(',', array_map(fn($x) => is_object($x) ? get_class($x) : gettype($x), $a));
    return 0;
});
var_dump(curl_exec($q) !== false);
echo '  ', $shape, "\n";

// a non-zero return aborts the transfer
$z = curl_init($url);
curl_setopt($z, CURLOPT_RETURNTRANSFER, true);
curl_setopt($z, CURLOPT_NOPROGRESS, false);
curl_setopt($z, CURLOPT_XFERINFOFUNCTION, fn(...$a) => 1);
var_dump(curl_exec($z), curl_errno($z) === CURLE_ABORTED_BY_CALLBACK);

unlink($tmp);
--EXPECT--
== the write callback's arguments ==
  argc=2 types=CurlHandle,string
  chunk='line
line
line
'
bool(true)
== the return value is a byte count ==
  return 0      => exec false errno 23
  return -1     => exec false errno 23
  return 'x'    => exec false errno 23
  return NULL   => exec false errno 23
  return true   => exec false errno 23
== a callback that throws ==
  exec =>   RuntimeException: from the callback
  errno after the throw: 0
== the shapes setopt takes ==
  closure        => true
  string         => true
  array-obj      => true
  array-static   => true
  string-static  => true
  invokable      => true
  null           => true
  missing-fn     => TypeError: curl_setopt(): Argument #3 ($value) must be a valid callback for option CURLOPT_WRITEFUNCTION, function "no_such_function_curlcb" not found or invalid function name
  not-callable   => TypeError: curl_setopt(): Argument #3 ($value) must be a valid callback for option CURLOPT_WRITEFUNCTION, no array or string given
  array-bad      => TypeError: curl_setopt(): Argument #3 ($value) must be a valid callback for option CURLOPT_WRITEFUNCTION, array callback must have exactly two members
== the progress callback ==
bool(true)
  5:CurlHandle,integer,integer,integer,integer
bool(false)
bool(true)
