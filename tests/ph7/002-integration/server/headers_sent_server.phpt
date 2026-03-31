--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
headers_sent() returns false in server mode (buffered output)
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
$port = 19900 + (getmypid() % 100);
mkdir($tmpdir);
$script = '<?php
echo "before:" . (headers_sent() ? "true" : "false") . "\n";
echo "output\n";
echo "after:" . (headers_sent() ? "true" : "false") . "\n";
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
echo trim($out);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/t.php');
rmdir($tmpdir);
?>
--EXPECT--
before:false
output
after:false
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $script, $fp, $pid, $fp2, $out);
