--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server respects header() calls
--SKIPIF--
<?php
$fp = popen("curl --version 2>/dev/null", "r");
$out = fgets($fp);
fclose($fp);
if (strlen($out) == 0) { echo "skip curl not available"; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_srvtest_' . getmypid();
$port = 19500 + (getmypid() % 100);
mkdir($tmpdir);
file_put_contents($tmpdir . '/hdr.php', '<?php header("Content-Type: application/json"); header("X-Custom: hello"); echo "{}";');

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s -D - http://localhost:' . $port . '/hdr.php 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);

$hasJson = strpos($out, 'Content-Type: application/json') !== false;
$hasCustom = strpos($out, 'X-Custom: hello') !== false;
echo $hasJson ? "content-type ok" : "content-type missing";
echo "\n";
echo $hasCustom ? "custom header ok" : "custom header missing";

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/hdr.php');
rmdir($tmpdir);
?>
--EXPECT--
content-type ok
custom header ok
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $out, $hasJson, $hasCustom);
