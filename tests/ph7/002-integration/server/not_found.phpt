--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server returns 404 for non-existent files
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip"; }
$fp = popen("curl --version 2>/dev/null", "r");
$out = fgets($fp);
fclose($fp);
if (strlen($out) == 0) { echo "skip curl not available"; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_srvtest_' . getmypid();
$port = 19400 + (getmypid() % 100);
mkdir($tmpdir);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s -o /dev/null -w "%{http_code}" http://localhost:' . $port . '/does_not_exist.php 2>/dev/null', 'r');
$code = '';
while (!feof($fp2)) { $code .= fgets($fp2); }
fclose($fp2);
echo trim($code);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
rmdir($tmpdir);
?>
--EXPECT--
404
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $code);
