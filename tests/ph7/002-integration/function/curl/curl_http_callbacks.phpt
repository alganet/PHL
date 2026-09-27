--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The transfer callbacks, and HTTP authentication, over a real socket
--DESCRIPTION--
The callbacks already answer over file://, where there is no header to read,
no status line, no upload and no second phase for the progress callback to
report. Over http:// they carry all of it, and three rules only a real response
shows:

  * a WRITEFUNCTION that returns a count other than the chunk's own length
    stops the transfer with CURLE_WRITE_ERROR (23) mid-body, so a partial body
    is what the caller keeps;
  * a callback that THROWS comes out of curl_exec as the original exception and
    leaves curl_errno at 0 -- the parked status wins over the CURLE_WRITE_ERROR
    the stopping return would otherwise have set;
  * the progress callback is asked before any byte arrives, so its first call
    is all zeroes, and a non-zero return is CURLE_ABORTED_BY_CALLBACK (42).

HTTP authentication is here for the same reason: the 401 challenge, the retry
carrying the credential and CURLINFO_HTTP_CODE across the two of them are only
visible against something that answers.
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

$port = curl_test_server_start($proc, 19700);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

function newHandle($url)
{
    $h = curl_init($url);
    curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($h, CURLOPT_TIMEOUT, 10);
    return $h;
}

/* 1. the write callback replaces the destination entirely */
$got = '';
$h = newHandle($base . '/headers');
curl_setopt($h, CURLOPT_WRITEFUNCTION, function ($handle, $chunk) use (&$got) {
    $got .= $chunk;
    return strlen($chunk);
});
printf("-- write cb: exec=%s collected=%s errno=%d\n",
    var_export(curl_exec($h), true), var_export($got, true), curl_errno($h));

/* 2. a short count stops the transfer */
$h = newHandle($base . '/headers');
curl_setopt($h, CURLOPT_WRITEFUNCTION, function ($handle, $chunk) {
    return 1;
});
printf("-- short return: exec=%s errno=%d\n", var_export(curl_exec($h), true), curl_errno($h));

/* 3. a throw travels out of curl_exec, and leaves no libcurl error behind */
$h = newHandle($base . '/headers');
curl_setopt($h, CURLOPT_WRITEFUNCTION, function ($handle, $chunk) {
    throw new RuntimeException('from the sink');
});
try {
    curl_exec($h);
    echo "-- throwing cb: no exception\n";
} catch (Throwable $e) {
    printf("-- throwing cb: %s: %s errno=%d\n", get_class($e), $e->getMessage(), curl_errno($h));
}

/* 4. the header callback counts the lines of a redirect chain */
$lines = array();
$h = newHandle($base . '/redirect');
curl_setopt($h, CURLOPT_FOLLOWLOCATION, true);
curl_setopt($h, CURLOPT_HEADERFUNCTION, function ($handle, $line) use (&$lines) {
    if (trim($line) !== '') {
        $lines[] = trim($line);
    }
    return strlen($line);
});
curl_exec($h);
printf("-- header cb over a redirect: statuses=%s count=%d\n",
    implode(',', array_values(array_filter($lines, function ($l) {
        return strpos($l, 'HTTP/') === 0;
    }))), count($lines));

/* 5. the progress callback: first call before any byte, non-zero aborts */
$calls = array();
$h = newHandle($base . '/headers');
curl_setopt($h, CURLOPT_NOPROGRESS, false);
curl_setopt($h, CURLOPT_XFERINFOFUNCTION, function ($handle, $dt, $dn, $ut, $un) use (&$calls) {
    $calls[] = "$dt/$dn/$ut/$un";
    return 0;
});
curl_exec($h);
printf("-- progress: first=%s calls>0=%s\n", var_export($calls[0], true), var_export(count($calls) > 0, true));

$h = newHandle($base . '/headers');
curl_setopt($h, CURLOPT_NOPROGRESS, false);
curl_setopt($h, CURLOPT_XFERINFOFUNCTION, function ($handle, $dt, $dn, $ut, $un) {
    return 1;
});
printf("-- progress abort: exec=%s errno=%d\n", var_export(curl_exec($h), true), curl_errno($h));

/* 6. the 401 challenge, and the retry that carries the credential */
$h = newHandle($base . '/auth');
printf("-- unauthenticated: body=%s code=%d\n", var_export(curl_exec($h), true),
    curl_getinfo($h, CURLINFO_HTTP_CODE));

$h = newHandle($base . '/auth');
curl_setopt($h, CURLOPT_USERPWD, 'user:pass');
curl_setopt($h, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
printf("-- basic: body=%s code=%d\n", var_export(curl_exec($h), true),
    curl_getinfo($h, CURLINFO_HTTP_CODE));

curl_test_server_stop($proc, $port);
?>
--EXPECT--
-- write cb: exec=true collected='headed' errno=0
-- short return: exec=false errno=23
-- throwing cb: RuntimeException: from the sink errno=0
-- header cb over a redirect: statuses=HTTP/1.1 302 Found,HTTP/1.1 200 OK count=8
-- progress: first='0/0/0/0' calls>0=true
-- progress abort: exec=false errno=42
-- unauthenticated: body='who?' code=401
-- basic: body='hello Basic user:pass' code=200
--CLEAN--
<?php
unset($port, $base, $proc, $h, $got, $lines, $calls);
?>
