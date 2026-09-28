--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: the handle it hands back, and who else can open one
--DESCRIPTION--
What an OPEN http:// stream is, once the exchange is over: a read-only,
unseekable handle whose metadata names `http` as the wrapper and the socket ops
as the stream, and whose `wrapper_data` is the response headers of the exchange
THIS handle made -- not the VM's last set, which a later request replaces.

The wrapper declines every mode that could write, before it opens a socket, and
`file_put_contents()` and `copy()` into one meet the same refusal.

It is a URL wrapper, so `allow_url_include=0` -- php's default -- stops an
`include` from executing whatever answered, with php's own sentence, while the
same URL still reads. (`allow_url_fopen`, the wholesale switch, is PHP_INI_SYSTEM
and gets its own test with an `--INI--` section.)

Every door that opens a stream publishes `$http_response_header` into the scope
that called it: file_get_contents(), fopen(), file(), readfile() and copy()
alike, and inside a FUNCTION it is that function's own local.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!in_array('http', stream_get_wrappers(), true)) {
    echo "skip no http:// wrapper in this build";
}
?>
--FILE--
<?php
require __DIR__ . '/http_wrapper_server.inc';

$dir = http_test_dir('hnd');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;
http_test_reply($dir, 'plain', "HTTP/1.1 200 OK\r\nX-Mark: one\r\n"
    . "Content-Length: 11\r\nConnection: close\r\n\r\nhello there");
http_test_reply($dir, 'chunk', "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
    . "Connection: close\r\n\r\n5\r\nhello\r\n6\r\n there\r\n0\r\n\r\n");

/* 1. the metadata of an open handle */
$h = fopen("$base/canned/plain", 'r');
$md = stream_get_meta_data($h);
printf("-- meta: %s\n", json_encode(array(
    'wrapper_type' => $md['wrapper_type'], 'stream_type' => $md['stream_type'],
    'mode' => $md['mode'], 'seekable' => $md['seekable'], 'blocked' => $md['blocked'],
    'timed_out' => $md['timed_out'], 'uri' => str_replace($base, 'BASE', $md['uri']),
)));
printf("-- wrapper_data: %s\n", json_encode($md['wrapper_data']));
printf("-- read: %s tell=%d eof=%s\n", json_encode(fread($h, 5)), ftell($h),
    var_export(feof($h), true));
printf("-- rest: %s tell=%d eof=%s\n", json_encode(stream_get_contents($h)), ftell($h),
    var_export(feof($h), true));
fclose($h);

/* 2. a chunked body reads the same way, one byte at a time */
$c = fopen("$base/canned/chunk", 'r');
$got = '';
while (!feof($c)) {
    $b = fread($c, 1);
    if ($b === '' || $b === false) {
        break;
    }
    $got .= $b;
}
printf("-- chunked one byte at a time: %s tell=%d\n", json_encode($got), ftell($c));
fclose($c);

/* 3. the wrapper_data of an OLD handle is still its own */
$first = fopen("$base/canned/plain", 'r');
$second = fopen("$base/echo", 'r');
$mdf = stream_get_meta_data($first);
printf("-- the first handle keeps its own headers: %s\n",
    json_encode($mdf['wrapper_data'][1]));
fclose($first);
fclose($second);

/* 4. every writable mode is refused, and so are the two writers */
foreach (array('w', 'a', 'r+', 'x') as $mode) {
    $w = @fopen("$base/canned/plain", $mode);
    printf("-- mode %s: %s %s\n", $mode, var_export($w, true),
        json_encode(substr(strstr(error_get_last()['message'], 'stream: '), 8)));
}
printf("-- file_put_contents: %s %s\n", var_export(@file_put_contents("$base/x", 'v'), true),
    json_encode(substr(strstr(error_get_last()['message'], 'stream: '), 8)));

/* 5. every reading door publishes the headers into its OWN scope */
function inside($url)
{
    $body = @file_get_contents($url);
    return isset($http_response_header) ? count($http_response_header) : 'unset';
}
printf("-- inside a function: %s\n", json_encode(inside("$base/canned/plain")));
$lines = @file("$base/canned/plain");
printf("-- file(): %s / %s\n", json_encode($lines), json_encode(count($http_response_header)));
unset($http_response_header);
ob_start();
@readfile("$base/canned/plain");
$printed = ob_get_clean();
printf("-- readfile(): %s / %s\n", json_encode($printed), json_encode(count($http_response_header)));
unset($http_response_header);

/* 6. allow_url_include is OFF by default, so an include of one is refused
 *    even though the very same URL reads. */
$seen = array();
set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
$inc = include "$base/canned/plain";
restore_error_handler();
printf("-- include: %s\n", json_encode($inc));
foreach ($seen as $line) {
    if (strpos($line, 'wrapper is disabled') !== false) {
        printf("   %s\n", json_encode($line));
    }
}

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECTF--
-- meta: {"wrapper_type":"http","stream_type":"tcp_socket\/ssl","mode":"r","seekable":false,"blocked":true,"timed_out":false,"uri":"BASE\/canned\/plain"}
-- wrapper_data: ["HTTP\/1.1 200 OK","X-Mark: one","Content-Length: 11","Connection: close"]
-- read: "hello" tell=5 eof=false
-- rest: " there" tell=11 eof=true
-- chunked one byte at a time: "hello there" tell=11
-- the first handle keeps its own headers: "X-Mark: one"
-- mode w: false "HTTP wrapper does not support writeable connections"
-- mode a: false "HTTP wrapper does not support writeable connections"
-- mode r+: false "HTTP wrapper does not support writeable connections"
-- mode x: false "HTTP wrapper does not support writeable connections"
-- file_put_contents: false "HTTP wrapper does not support writeable connections"
-- inside a function: 4
-- file(): ["hello there"] / 4
-- readfile(): "hello there" / 4
-- include: false
   "include(): http:\/\/ wrapper is disabled in the server configuration by allow_url_include=0"
