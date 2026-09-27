--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_exec() over http://: the request written, the response read back
--DESCRIPTION--
The http:// half of the transfer surface, which until now waited on something
to talk to. `http_server.inc` is that something -- a raw socket server spawned
as a child of the test, in the engine under test -- and `/echo` answers the
request BYTES back, so the assertions here are about what ext/curl actually put
on the wire and not only about what it answered afterwards.

What only a real response can show: the status line becomes CURLINFO_HTTP_CODE
and not an error (a 404 is a SUCCESSFUL transfer -- curl_exec answers the body
and curl_errno stays 0, which is the single most common wrong assumption about
this API), a 302 is followed only with CURLOPT_FOLLOWLOCATION and then reports
its own hop count, a chunked body arrives decoded with no length known in
advance, the header callback sees the status line as a header of its own and
the trailing empty line too, and a body that stops arriving is
CURLE_OPERATION_TIMEDOUT rather than a short read.

The volatile parts of the request are scrubbed before printing: the Host line
carries this run's port, and libcurl's own default headers are the LIBRARY's,
so only their shape is pinned.
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

$port = curl_test_server_start($proc);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

/* 1. the request bytes, and the body that came back */
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
$raw = curl_test_request(curl_exec($h));
echo "-- request as sent\n", curl_test_scrub($raw);
printf("errno=%d code=%d type=%s size=%d\n", curl_errno($h), curl_getinfo($h, CURLINFO_HTTP_CODE),
    var_export(curl_getinfo($h, CURLINFO_CONTENT_TYPE), true), curl_getinfo($h, CURLINFO_SIZE_DOWNLOAD));

/* 2. custom headers ride on the same request */
curl_setopt($h, CURLOPT_HTTPHEADER, array('X-Test: yes', 'Accept: text/plain'));
echo "-- with CURLOPT_HTTPHEADER\n", curl_test_scrub(curl_test_request(curl_exec($h)));

/* 3. RETURNTRANSFER off sends the body to the script's own output */
$g = curl_init($base . '/status/200');
curl_setopt($g, CURLOPT_TIMEOUT, 10);
ob_start();
$r = curl_exec($g);
$printed = ob_get_clean();
printf("-- returntransfer off: %s printed=%s\n", var_export($r, true), var_export($printed, true));

/* 4. a 404 is a successful transfer */
$n = curl_init($base . '/status/404');
curl_setopt($n, CURLOPT_RETURNTRANSFER, true);
curl_setopt($n, CURLOPT_TIMEOUT, 10);
printf("-- 404: body=%s errno=%d code=%d\n", var_export(curl_exec($n), true),
    curl_errno($n), curl_getinfo($n, CURLINFO_HTTP_CODE));

/* 5. a redirect, unfollowed and followed */
$d = curl_init($base . '/redirect2');
curl_setopt($d, CURLOPT_RETURNTRANSFER, true);
curl_setopt($d, CURLOPT_TIMEOUT, 10);
printf("-- unfollowed: body=%s code=%d redirects=%d\n", var_export(curl_exec($d), true),
    curl_getinfo($d, CURLINFO_HTTP_CODE), curl_getinfo($d, CURLINFO_REDIRECT_COUNT));
curl_setopt($d, CURLOPT_FOLLOWLOCATION, true);
$body = curl_test_request(curl_exec($d));
printf("-- followed: code=%d redirects=%d final=%s\n", curl_getinfo($d, CURLINFO_HTTP_CODE),
    curl_getinfo($d, CURLINFO_REDIRECT_COUNT),
    var_export(substr(strstr($body, ' '), 1, 5), true));

/* 6. the response headers, through the header callback */
$c = curl_init($base . '/headers');
curl_setopt($c, CURLOPT_RETURNTRANSFER, true);
curl_setopt($c, CURLOPT_TIMEOUT, 10);
$seen = array();
curl_setopt($c, CURLOPT_HEADERFUNCTION, function ($handle, $line) use (&$seen) {
    $seen[] = $line;
    return strlen($line);
});
printf("-- headers: body=%s\n", var_export(curl_exec($c), true));
foreach ($seen as $line) {
    echo '   [', str_replace("\r\n", '\r\n', $line), "]\n";
}

/* 7. a chunked body arrives decoded, with no length known in advance */
$k = curl_init($base . '/chunked');
curl_setopt($k, CURLOPT_RETURNTRANSFER, true);
curl_setopt($k, CURLOPT_TIMEOUT, 10);
printf("-- chunked: body=%s declared=%s downloaded=%d\n", var_export(curl_exec($k), true),
    var_export(curl_getinfo($k, CURLINFO_CONTENT_LENGTH_DOWNLOAD), true),
    curl_getinfo($k, CURLINFO_SIZE_DOWNLOAD));

/* 8. a body that stops arriving is a timeout, not a short read */
$s = curl_init($base . '/slow');
curl_setopt($s, CURLOPT_RETURNTRANSFER, true);
curl_setopt($s, CURLOPT_TIMEOUT, 1);
printf("-- slow: body=%s errno=%d\n", var_export(curl_exec($s), true), curl_errno($s));

/* 9. CURLOPT_NOBODY asks for the head of it only */
$b = curl_init($base . '/echo');
curl_setopt($b, CURLOPT_RETURNTRANSFER, true);
curl_setopt($b, CURLOPT_TIMEOUT, 10);
curl_setopt($b, CURLOPT_NOBODY, true);
printf("-- nobody: body=%s code=%d\n", var_export(curl_exec($b), true),
    curl_getinfo($b, CURLINFO_HTTP_CODE));

curl_test_server_stop($proc, $port);
?>
--EXPECT--
-- request as sent
GET /echo HTTP/1.1
Host: SERVER
Accept: */*

errno=0 code=200 type='text/plain' size=116
-- with CURLOPT_HTTPHEADER
GET /echo HTTP/1.1
Host: SERVER
X-Test: yes
Accept: text/plain

-- returntransfer off: true printed='status 200'
-- 404: body='status 404' errno=0 code=404
-- unfollowed: body='' code=302 redirects=0
-- followed: code=200 redirects=2 final='/echo'
-- headers: body='headed'
   [HTTP/1.1 200 OK\r\n]
   [Content-Type: text/plain; charset=iso-8859-1\r\n]
   [X-First: one\r\n]
   [X-Second: two\r\n]
   [Content-Length: 6\r\n]
   [Connection: close\r\n]
   [\r\n]
-- chunked: body='chunked!' declared=-1.0 downloaded=8
-- slow: body=false errno=28
-- nobody: body='' code=200
--CLEAN--
<?php
unset($port, $base, $proc, $h, $g, $n, $d, $c, $k, $s, $b, $raw, $body, $seen, $printed, $r, $line);
?>
