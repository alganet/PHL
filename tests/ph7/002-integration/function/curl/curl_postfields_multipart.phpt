--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CURLOPT_POSTFIELDS with an array: the multipart body, and the uploads in it
--DESCRIPTION--
An ARRAY is a different request from a string: multipart/form-data, built out
of libcurl's own mime API, so the BOUNDARY is the library's and no test may pin
it (this one scrubs it).

php's walk is one level deep and no more. Each entry is a part named by its KEY
-- an integer key in decimal, a key truncated at a NUL because the name reaches
libcurl as a C string -- and a value that is itself an array is walked once
more with the OUTER key repeated, so `['a' => ['x','y']]` is two parts both
named "a". A THIRD level has no rule of its own: the value is stringified, which
is the ordinary "Array to string conversion" warning and the five letters
"Array". An EMPTY array is not multipart at all: php sets the ordinary empty
string, so the request is a zero-length urlencoded POST with no boundary
anywhere.

A CURLFile is read through the STREAM layer and not by libcurl, which is worth
three answers of its own: `php://temp` and `data://` are legal upload sources, a
file unlinked between setopt and exec still uploads (the handle is already
open), and a file that cannot be READ is php's notice naming the size and the
errno, then CURLE_ABORTED_BY_CALLBACK -- which is how a directory is told from
an empty file.

A copied handle REBUILDS the whole body from the array php keeps for that
purpose, because a mime whose parts read through callbacks cannot be shared:
the copy re-opens every CURLFile, and a copy made after the file was unlinked
fails where the original still succeeds.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

class CurlPartStringable
{
    public function __toString(): string
    {
        return 'str-obj';
    }
}

/* Two diagnostics ride on this test -- an "Array to string conversion" warning
 * and the read failure a directory answers -- and both print the running FILE's
 * path, which no expectation can carry. The handler is error_reporting-aware so
 * that the @-suppressed probes inside the server helper stay suppressed. */
set_error_handler(static function ($no, $str) {
    if ((error_reporting() & $no) === 0) {
        return true;
    }
    /* the size a failed read names is what libcurl asked the read callback
     * for, which is the library build's own */
    echo '  [', $no, '] ', preg_replace('/Read of \d+ bytes/', 'Read of N bytes', $str), "\n";
    return true;
});

/* the parts below carry the file's path as their filename, so the
 * Content-Length this test pins is corrected for the directory's length where
 * the body is printed */
$dir = sys_get_temp_dir() . '/phl_curl_mime_' . str_pad((string) getmypid(), 8, '0', STR_PAD_LEFT);
@mkdir($dir);
file_put_contents($dir . '/up.txt', "hello file\n");
file_put_contents($dir . '/empty.bin', '');

$port = curl_test_server_start($proc, 19640);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

$send = static function ($value) use ($base) {
    $h = curl_init($base . '/echo');
    curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($h, CURLOPT_TIMEOUT, 10);
    try {
        curl_setopt($h, CURLOPT_POSTFIELDS, $value);
    } catch (Throwable $e) {
        echo '  ', get_class($e), ': ', $e->getMessage(), "\n";
    }
    $out = curl_exec($h);
    if ($out === false) {
        printf("  exec=false errno=%d\n", curl_errno($h));
        return;
    }
    $raw = curl_test_scrub(curl_test_request($out));
    /* the temp dir's length is the platform's, so the Content-Length pinned is
     * the one the body would have with DIR in the path's place */
    $shrink = substr_count($raw, $GLOBALS['dir']) * (strlen($GLOBALS['dir']) - strlen('DIR'));
    $raw = preg_replace_callback('/^Content-Length: (\d+)/m', static function ($m) use ($shrink) {
        return 'Content-Length: ' . ($m[1] - $shrink);
    }, $raw);
    $raw = str_replace($GLOBALS['dir'], 'DIR', $raw);
    /* the Host line carries this run's port, the two default headers are the
     * library's: only the body shape is this test's business */
    $raw = preg_replace("/^(Host|Accept): [^\n]*\n/m", '', $raw);
    echo '  ', str_replace(array("\n", "\0"), array("\n  ", '<NUL>'), rtrim($raw)), "\n";
};

foreach (array(
    'empty array' => array(),
    'assoc' => array('a' => '1', 'b' => '2'),
    'list' => array(1, 2),
    'int key' => array(5 => 'v'),
    'NUL in key' => array("k\0x" => 'v'),
    'NUL in value' => array('a' => "x\0y"),
    'null value' => array('a' => null),
    'bools' => array('a' => true, 'b' => false),
    'stringable' => array('a' => new CurlPartStringable()),
    'nested' => array('a' => array('x', 'y'), 'd' => '3'),
    'nested twice' => array('a' => array('b' => array('c' => 'deep'))),
) as $label => $value) {
    echo '== ', $label, "\n";
    $send($value);
}

echo "== a file\n";
$send(array('f' => new CURLFile($dir . '/up.txt')));
echo "== a file with a type and a posted name\n";
$send(array('f' => new CURLFile($dir . '/up.txt', 'text/plain', 'renamed.txt')));
echo "== an empty file\n";
$send(array('f' => new CURLFile($dir . '/empty.bin')));
echo "== a file beside a field\n";
$send(array('a' => '1', 'f' => new CURLFile($dir . '/up.txt')));
echo "== a stream wrapper as the source\n";
$send(array('f' => new CURLFile('data://text/plain,from-a-wrapper')));
echo "== a missing file\n";
$send(array('f' => new CURLFile($dir . '/nope.txt')));
echo "== a directory\n";
$send(array('f' => new CURLFile($dir)));
echo "== a string file\n";
$send(array('f' => new CURLStringFile("payload\0bytes", 's.bin')));
echo "== a string file with a type\n";
$send(array('f' => new CURLStringFile('payload', 's.txt', 'text/plain')));

/* the file is opened when the option is SET */
echo "== unlinked between setopt and exec\n";
file_put_contents($dir . '/gone.txt', 'STILL HERE');
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_POSTFIELDS, array('f' => new CURLFile($dir . '/gone.txt')));
unlink($dir . '/gone.txt');
$raw = curl_test_request(curl_exec($h));
printf("  uploaded=%s\n", var_export($raw !== false && strpos($raw, 'STILL HERE') !== false, true));

/* a copy rebuilds it, and re-opens what it names */
echo "== a copy rebuilds the body\n";
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_POSTFIELDS, array('f' => new CURLFile($dir . '/up.txt'), 'k' => 'v'));
$copy = curl_copy_handle($h);
$one = curl_test_scrub(curl_test_request(curl_exec($h)));
$two = curl_test_scrub(curl_test_request(curl_exec($copy)));
printf("  same body=%s\n", var_export($one === $two, true));

echo "== a copy re-opens what the array names\n";
file_put_contents($dir . '/vanish.txt', 'x');
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_POSTFIELDS, array('f' => new CURLFile($dir . '/vanish.txt')));
unlink($dir . '/vanish.txt');
$copy = curl_copy_handle($h);
printf("  copy=%s\n", var_export(curl_exec($copy) !== false, true));

/* a plain body replaces a multipart one, and the other way round */
echo "== array then string\n";
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_POSTFIELDS, array('a' => '1'));
curl_setopt($h, CURLOPT_POSTFIELDS, 'plain=1');
printf("  %s\n", str_replace("\n", ' | ', trim(curl_test_scrub(curl_test_request(curl_exec($h))))));
echo "== string then array\n";
curl_setopt($h, CURLOPT_POSTFIELDS, array('a' => '1'));
$raw = curl_test_scrub(curl_test_request(curl_exec($h)));
printf("  multipart=%s\n", var_export(strpos($raw, 'multipart/form-data') !== false, true));
echo "== twice over\n";
curl_setopt($h, CURLOPT_POSTFIELDS, array('b' => '2'));
$raw = curl_test_scrub(curl_test_request(curl_exec($h)));
printf("  names=%s\n", implode(',', array_map(static function ($m) {
    return $m;
}, preg_match_all('/name="([^"]*)"/', $raw, $m) ? $m[1] : array())));

curl_test_server_stop($proc, $port);
@unlink($dir . '/up.txt');
@unlink($dir . '/empty.bin');
@rmdir($dir);
?>
--EXPECT--
== empty array
  POST /echo HTTP/1.1
  Content-Length: 0
  Content-Type: application/x-www-form-urlencoded
== assoc
  POST /echo HTTP/1.1
  Content-Length: 246
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  1
  --BOUNDARY--
  Content-Disposition: form-data; name="b"
  
  2
  --BOUNDARY----
== list
  POST /echo HTTP/1.1
  Content-Length: 246
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="0"
  
  1
  --BOUNDARY--
  Content-Disposition: form-data; name="1"
  
  2
  --BOUNDARY----
== int key
  POST /echo HTTP/1.1
  Content-Length: 149
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="5"
  
  v
  --BOUNDARY----
== NUL in key
  POST /echo HTTP/1.1
  Content-Length: 149
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="k"
  
  v
  --BOUNDARY----
== NUL in value
  POST /echo HTTP/1.1
  Content-Length: 151
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  x<NUL>y
  --BOUNDARY----
== null value
  POST /echo HTTP/1.1
  Content-Length: 148
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  
  --BOUNDARY----
== bools
  POST /echo HTTP/1.1
  Content-Length: 245
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  1
  --BOUNDARY--
  Content-Disposition: form-data; name="b"
  
  
  --BOUNDARY----
== stringable
  POST /echo HTTP/1.1
  Content-Length: 155
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  str-obj
  --BOUNDARY----
== nested
  POST /echo HTTP/1.1
  Content-Length: 343
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  x
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  y
  --BOUNDARY--
  Content-Disposition: form-data; name="d"
  
  3
  --BOUNDARY----
== nested twice
  [2] Array to string conversion
  POST /echo HTTP/1.1
  Content-Length: 153
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  Array
  --BOUNDARY----
== a file
  POST /echo HTTP/1.1
  Content-Length: 222
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="DIR/up.txt"
  Content-Type: application/octet-stream
  
  hello file
  
  --BOUNDARY----
== a file with a type and a posted name
  POST /echo HTTP/1.1
  Content-Length: 209
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="renamed.txt"
  Content-Type: text/plain
  
  hello file
  
  --BOUNDARY----
== an empty file
  POST /echo HTTP/1.1
  Content-Length: 214
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="DIR/empty.bin"
  Content-Type: application/octet-stream
  
  
  --BOUNDARY----
== a file beside a field
  POST /echo HTTP/1.1
  Content-Length: 319
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="a"
  
  1
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="DIR/up.txt"
  Content-Type: application/octet-stream
  
  hello file
  
  --BOUNDARY----
== a stream wrapper as the source
  POST /echo HTTP/1.1
  Content-Length: 247
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="data://text/plain,from-a-wrapper"
  Content-Type: application/octet-stream
  
  from-a-wrapper
  --BOUNDARY----
== a missing file
  exec=false errno=42
== a directory
  [8] curl_exec(): Read of N bytes failed with errno=21 Is a directory
  exec=false errno=42
== a string file
  POST /echo HTTP/1.1
  Content-Length: 219
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="s.bin"
  Content-Type: application/octet-stream
  
  payload<NUL>bytes
  --BOUNDARY----
== a string file with a type
  POST /echo HTTP/1.1
  Content-Length: 199
  Content-Type: multipart/form-data; boundary=--BOUNDARY--
  
  --BOUNDARY--
  Content-Disposition: form-data; name="f"; filename="s.txt"
  Content-Type: text/plain
  
  payload
  --BOUNDARY----
== unlinked between setopt and exec
  uploaded=true
== a copy rebuilds the body
  same body=true
== a copy re-opens what the array names
  copy=false
== array then string
  POST /echo HTTP/1.1 | Host: SERVER | Accept: */* | Content-Length: 7 | Content-Type: application/x-www-form-urlencoded |  | plain=1
== string then array
  multipart=true
== twice over
  names=b
--CLEAN--
<?php
unset($dir, $port, $base, $proc, $send, $h, $copy, $one, $two, $raw, $label, $value, $m, $e);
?>
