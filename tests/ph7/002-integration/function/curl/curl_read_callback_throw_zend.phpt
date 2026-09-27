--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a throwing READFUNCTION stalls the upload until the timeout (zend half of the twin pair)
--DESCRIPTION--
The zend half of the pair. php's read callback answers zero bytes when the php
it called threw, and libcurl goes on waiting for the length the transfer
declared -- so the handle ends at CURLE_OPERATION_TIMEDOUT and the script pays
the whole CURLOPT_TIMEOUT before its own exception arrives. A handle with no
timeout set waits for good.

PHL lets the short upload finish instead; the PHL half of the pair pins that.

What libcurl does with the short read is the library build's answer, not php's:
macOS's ends the transfer at once with CURLE_READ_ERROR. What is recorded here
is php over Linux's libcurl.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
} elseif (PHP_OS_FAMILY !== 'Linux') {
    echo "skip records php over Linux's libcurl";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

$port = curl_test_server_start($proc, 19740);
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
RuntimeException: from the reader errno=28 waited-for-the-timeout=true
--CLEAN--
<?php
unset($port, $proc, $h, $started, $e);
?>
