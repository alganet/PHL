--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The built-in server defines PHP_BINARY in served VMs
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') { echo "skip"; }
$fp = popen("curl --version 2>/dev/null", "r");
$out = fgets($fp);
fclose($fp);
if (strlen($out) == 0) { echo "skip curl not available"; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_binsrv_' . getmypid();
$port = 19500 + (getmypid() % 100);
mkdir($tmpdir);
file_put_contents($tmpdir . '/bin.php',
    "<?php echo (defined('PHP_BINARY') && PHP_BINARY !== '' && is_file(PHP_BINARY)) ? \"ok\\n\" : \"bad\\n\";\n");

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s "http://localhost:' . $port . '/bin.php" 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);
echo $out;

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/bin.php');
rmdir($tmpdir);
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $out);
