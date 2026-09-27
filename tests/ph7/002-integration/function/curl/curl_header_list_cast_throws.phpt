--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A header list keeps what it built around an element that would not cast
--DESCRIPTION--
The slist half of the same rule the smoke corpus pins for a string option: the
cast of an element that has no string form throws AND answers an empty string,
and php's loop is a plain C loop that keeps walking. So the element becomes "",
the elements AFTER it are still appended, and the finished list still replaces
whatever the option held before -- which is only visible on the wire, because
nothing reads a header list back.

Dropping the list on the throw is the reading that looks careful and answers
wrongly twice over: the headers the caller did spell would not be sent, and the
PREVIOUS list would still be.
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

class CurlListNoString
{
}

$port = curl_test_server_start($proc, 19660);
if ($port === null) {
    echo "server did not start\n";
    return;
}

$h = curl_init('http://127.0.0.1:' . $port . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_HTTPHEADER, array('X-Replaced: yes'));
try {
    curl_setopt($h, CURLOPT_HTTPHEADER, array('X-Before: 1', new CurlListNoString(), 'X-After: 2'));
} catch (Throwable $e) {
    printf("caught: %s: %s\n", get_class($e), $e->getMessage());
}
printf("errno=%d\n", curl_errno($h));
echo curl_test_scrub(curl_test_request(curl_exec($h)));

curl_test_server_stop($proc, $port);
?>
--EXPECT--
caught: Error: Object of class CurlListNoString could not be converted to string
errno=0
GET /echo HTTP/1.1
Host: SERVER
Accept: */*
X-Before: 1
X-After: 2
--CLEAN--
<?php
unset($port, $proc, $h, $e);
?>
