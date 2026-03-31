--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server serves static files with correct content
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
$port = 19300 + (getmypid() % 100);
mkdir($tmpdir);
file_put_contents($tmpdir . '/hello.txt', 'static content here');

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s http://localhost:' . $port . '/hello.txt 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);

$fp3 = popen('curl -s -o /dev/null -w "%{content_type}" http://localhost:' . $port . '/hello.txt 2>/dev/null', 'r');
$ct = '';
while (!feof($fp3)) { $ct .= fgets($fp3); }
fclose($fp3);

echo trim($out) . "\n";
echo trim($ct);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/hello.txt');
rmdir($tmpdir);
?>
--EXPECT--
static content here
text/plain
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $fp3, $out, $ct);
