--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server serves PHP script with correct superglobals
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
$port = 19200 + (getmypid() % 100);
mkdir($tmpdir);
file_put_contents($tmpdir . '/test.php', '<?php echo $_SERVER["REQUEST_METHOD"] . " " . $_SERVER["REQUEST_URI"];');

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s http://localhost:' . $port . '/test.php?foo=bar 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);
echo trim($out);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/test.php');
rmdir($tmpdir);
?>
--EXPECT--
GET /test.php?foo=bar
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $out);
