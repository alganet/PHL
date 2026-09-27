--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A transfer pointed at php streams: the body, the headers, the trace and the upload
--DESCRIPTION--
What the five stream options DO, which needs a real response for four of them.

The body goes to CURLOPT_FILE and curl_exec answers true. The response headers
go to CURLOPT_WRITEHEADER raw, blank line and all -- and they go there whether
or not CURLOPT_HEADER asked for them in the body, because php installs a header
callback on every transfer and simply drops what nothing asked for. The upload
reads from CURLOPT_INFILE, or from a CURLOPT_READFUNCTION which is handed that
same stream as its second argument, so the documented idiom is a callback that
fread()s the handle it was given.

CURLOPT_STDERR is libcurl's verbose trace, and the two spellings of it are one
setting: a CURLOPT_DEBUGFUNCTION takes the callback outright and then nothing
reaches the stream at all, which is libcurl's own precedence. Its TEXT is
libcurl's and changes between versions, so only its shape is pinned -- the
prefixes it uses, and the request line appearing under the outgoing-header one.
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

$dir = sys_get_temp_dir() . '/phl_curl_streams_' . getmypid();
@mkdir($dir);

$port = curl_test_server_start($proc, 19710);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

/* the body to a file */
$bodyFile = $dir . '/body.txt';
$f = fopen($bodyFile, 'w');
$h = curl_init($base . '/headers');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_FILE, $f);
printf("body to a file: exec=%s content=%s\n", var_export(curl_exec($h), true),
    var_export((fclose($f) === true) ? file_get_contents($bodyFile) : '?', true));

/* the headers to a file, with the body answered as usual */
$hdrFile = $dir . '/head.txt';
$g = fopen($hdrFile, 'w');
$h = curl_init($base . '/headers');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_WRITEHEADER, $g);
printf("headers to a file: exec=%s\n", var_export(curl_exec($h), true));
fclose($g);
echo str_replace("\r\n", "\n", file_get_contents($hdrFile));

/* the verbose trace to a stream */
$errFile = $dir . '/trace.txt';
$e = fopen($errFile, 'w');
$h = curl_init($base . '/headers');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_VERBOSE, true);
curl_setopt($h, CURLOPT_STDERR, $e);
curl_exec($h);
fclose($e);
$trace = file_get_contents($errFile);
printf("trace: request-line=%s status-line=%s body-absent=%s\n",
    var_export(strpos($trace, "> GET /headers HTTP/1.1") !== false, true),
    var_export(strpos($trace, "< HTTP/1.1 200 OK") !== false, true),
    var_export(strpos($trace, 'headed') === false, true));

/* a DEBUGFUNCTION takes the callback, and the stream then sees nothing */
$e = fopen($errFile, 'w');
$h = curl_init($base . '/headers');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_VERBOSE, true);
curl_setopt($h, CURLOPT_STDERR, $e);
$kinds = array();
curl_setopt($h, CURLOPT_DEBUGFUNCTION, function ($handle, $type, $data) use (&$kinds) {
    $kinds[$type] = true;
    return 0;
});
curl_exec($h);
fclose($e);
ksort($kinds);
printf("debug cb: types=%s stream=%s\n", implode(',', array_keys($kinds)),
    var_export(file_get_contents($errFile), true));

/* a debug callback without CURLOPT_VERBOSE is never called */
$called = 0;
$h = curl_init($base . '/headers');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_DEBUGFUNCTION, function ($handle, $type, $data) use (&$called) {
    $called++;
    return 0;
});
curl_exec($h);
printf("debug cb without verbose: called=%d\n", $called);

/* the upload source: a stream */
$up = $dir . '/up.txt';
file_put_contents($up, 'UPLOADED BODY');
$in = fopen($up, 'r');
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_UPLOAD, true);
curl_setopt($h, CURLOPT_INFILE, $in);
curl_setopt($h, CURLOPT_INFILESIZE, filesize($up));
$raw = curl_test_request(curl_exec($h));
fclose($in);
printf("infile: %s\n", str_replace("\r\n", ' | ', curl_test_scrub($raw)));

/* the upload source: a callback, handed the same stream */
$in = fopen($up, 'r');
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_UPLOAD, true);
curl_setopt($h, CURLOPT_INFILE, $in);
curl_setopt($h, CURLOPT_INFILESIZE, 13);
$seen = '';
curl_setopt($h, CURLOPT_READFUNCTION, function ($handle, $stream, $length) use (&$seen) {
    $seen = get_class($handle) . '/' . (is_resource($stream) ? 'resource' : gettype($stream));
    return strtolower((string) fread($stream, $length));
});
$raw = curl_test_request(curl_exec($h));
fclose($in);
printf("readfn args: %s\n", $seen);
printf("readfn body: %s\n", var_export(substr(strstr($raw, "\r\n\r\n"), 4), true));

/* a callback with no stream behind it is handed null */
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_UPLOAD, true);
curl_setopt($h, CURLOPT_INFILESIZE, 9);
$sent = false;
curl_setopt($h, CURLOPT_READFUNCTION, function ($handle, $stream, $length) use (&$sent) {
    if ($sent) {
        return '';
    }
    $sent = gettype($stream);
    return 'FROM A CB';
});
$raw = curl_test_request(curl_exec($h));
printf("readfn alone: stream=%s body=%s\n", var_export($sent, true),
    var_export(substr(strstr($raw, "\r\n\r\n"), 4), true));

curl_test_server_stop($proc, $port);
@unlink($bodyFile);
@unlink($hdrFile);
@unlink($errFile);
@unlink($up);
@rmdir($dir);
?>
--EXPECT--
body to a file: exec=true content='headed'
headers to a file: exec='headed'
HTTP/1.1 200 OK
Content-Type: text/plain; charset=iso-8859-1
X-First: one
X-Second: two
Content-Length: 6
Connection: close

trace: request-line=true status-line=true body-absent=true
debug cb: types=0,1,2,3 stream=''
debug cb without verbose: called=0
infile: PUT /echo HTTP/1.1
Host: SERVER
Accept: */*
Content-Length: 13

UPLOADED BODY
readfn args: CurlHandle/resource
readfn body: 'uploaded body'
readfn alone: stream='NULL' body='FROM A CB'
--CLEAN--
<?php
unset($dir, $port, $base, $proc, $h, $f, $g, $e, $in, $raw, $trace, $kinds, $called, $seen, $sent,
    $bodyFile, $hdrFile, $errFile, $up);
?>
