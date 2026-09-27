--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: copying a handle leaves the SOURCE's upload readable (PHL half of the twin pair)
--DESCRIPTION--
Both engines REBUILD a multipart body for a copied handle rather than share
one -- a mime whose file parts read through callbacks cannot be duplicated,
since the duplicate would read the source's own open streams. The rebuild
re-opens every CURLFile, so a copy made after the file went is a copy that
cannot upload it, in both engines.

They part company over what the rebuild does to the ORIGINAL. php's copy closes
the source's stream on its way past, so the handle that was copied stops being
able to upload the file it opened -- a transfer that worked before the copy
answers CURLE_ABORTED_BY_CALLBACK after it, without anything having been asked
of the original at all. PHL leaves the source alone: copying a handle is a read
of it.

The twin (`_zend`) half pins php's answer so the divergence is recorded rather
than silently accepted.
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

$dir = sys_get_temp_dir() . '/phl_curl_copy_' . getmypid();
@mkdir($dir);
file_put_contents($dir . '/up.txt', 'payload');

$port = curl_test_server_start($proc, 19620);
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
after unlink:  original=true copy=false
--CLEAN--
<?php
unset($dir, $port, $proc, $h, $copy, $late);
?>
