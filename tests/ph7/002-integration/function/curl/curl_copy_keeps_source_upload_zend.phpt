--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: copying a handle CLOSES the source's upload stream (zend half of the twin pair)
--DESCRIPTION--
The zend half of the pair: php's `curl_copy_handle` rebuilds the copy's
multipart body and CLOSES the source's open upload stream on the way, so a
handle that could upload its file before the copy cannot afterwards. Nothing
was asked of the original -- it was only read from.

PHL leaves the source alone; the PHL half of the pair pins that.

--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

$dir = sys_get_temp_dir() . '/php_curl_copy_' . getmypid();
@mkdir($dir);
file_put_contents($dir . '/up.txt', 'payload');

$port = curl_test_server_start($proc, 19630);
if ($port === null) {
    echo "server did not start\n";
    return;
}

$h = curl_init('http://127.0.0.1:' . $port . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
curl_setopt($h, CURLOPT_POSTFIELDS, array('f' => new CURLFile($dir . '/up.txt')));
$copy = curl_copy_handle($h);
printf("both readable: original=%s copy=%s\n",
    var_export(strpos(curl_test_request(curl_exec($h)), 'payload') !== false, true),
    var_export(strpos(curl_test_request(curl_exec($copy)), 'payload') !== false, true));

/* the rebuild re-opens, so a copy made after the file went cannot upload it --
 * and the source, which opened it while it was there, still can */
unlink($dir . '/up.txt');
$late = curl_copy_handle($h);
printf("after unlink:  original=%s copy=%s\n",
    var_export(curl_exec($h) !== false, true),
    var_export(curl_exec($late) !== false, true));

curl_test_server_stop($proc, $port);
@rmdir($dir);
?>
--EXPECT--
both readable: original=true copy=true
after unlink:  original=false copy=false
--CLEAN--
<?php
unset($dir, $port, $proc, $h, $copy, $late);
?>
