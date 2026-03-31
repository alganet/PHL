--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_response_code() works in server mode (set/get and HTTP status)
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
$port = 19700 + (getmypid() % 100);
mkdir($tmpdir);
$script = '<?php
echo http_response_code() . "\n";
echo http_response_code(201) . "\n";
echo http_response_code() . "\n";
';
file_put_contents($tmpdir . '/t.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s http://localhost:' . $port . '/t.php 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);

$fp3 = popen('curl -s -o /dev/null -w "%{http_code}" http://localhost:' . $port . '/t.php 2>/dev/null', 'r');
$code = '';
while (!feof($fp3)) { $code .= fgets($fp3); }
fclose($fp3);

echo trim($out) . "\n";
echo "status:" . trim($code);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/t.php');
rmdir($tmpdir);
?>
--EXPECT--
200
200
201
status:201
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $script, $fp, $pid, $fp2, $fp3, $out, $code);
