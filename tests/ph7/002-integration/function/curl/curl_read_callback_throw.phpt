--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a throwing READFUNCTION ends the upload rather than stalling it (PHL half of the twin pair)
--DESCRIPTION--
Both engines park the exception a read callback raises and let curl_exec throw
it once libcurl has unwound; the callback itself answers zero bytes, because a
read callback cannot abort the library the way a write callback can.

They differ in what libcurl then does. php leaves the transfer waiting for the
length it declared, so the handle ends at CURLE_OPERATION_TIMEDOUT and the
script pays the whole CURLOPT_TIMEOUT before seeing its own exception -- and a
handle with no timeout set waits forever. PHL lets the short upload finish, so
the exception arrives at once and the handle carries whatever code the transfer
really ended with.

The exception itself, the class and the message are identical; only the wait
and the code behind it are not. The twin (`_zend`) half pins php's.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

$port = curl_test_server_start($proc, 19730);
if ($port === null) {
    echo "server did not start\n";
    return;
}

$h = curl_init('http://127.0.0.1:' . $port . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 2);
curl_setopt($h, CURLOPT_UPLOAD, true);
curl_setopt($h, CURLOPT_INFILESIZE, 8);
curl_setopt($h, CURLOPT_READFUNCTION, function ($handle, $stream, $length) {
    throw new RuntimeException('from the reader');
});
$started = microtime(true);
try {
    curl_exec($h);
    echo "no exception\n";
} catch (Throwable $e) {
    printf("%s: %s errno=%d waited-for-the-timeout=%s\n", get_class($e), $e->getMessage(),
        curl_errno($h), var_export(microtime(true) - $started >= 1.5, true));
}

curl_test_server_stop($proc, $port);
?>
--EXPECT--
RuntimeException: from the reader errno=0 waited-for-the-timeout=false
--CLEAN--
<?php
unset($port, $proc, $h, $started, $e);
?>
