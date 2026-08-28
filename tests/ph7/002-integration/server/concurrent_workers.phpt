--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server: PHP_CLI_SERVER_WORKERS handles requests concurrently
--SKIPIF--
<?php
$fp = popen("curl --version 2>/dev/null", "r");
$out = fgets($fp);
fclose($fp);
if (strlen($out) == 0) { echo "skip curl not available"; }
if (!defined('PH7_VERSION')) { echo "skip PHL-only server flag"; }
if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') { echo "skip fork workers are POSIX-only"; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_cwtest_' . getmypid();
$port = 19400 + (getmypid() % 100);
mkdir($tmpdir);
// each request sleeps a second, so two sequential ones would take >= 2s
file_put_contents($tmpdir . '/slow.php', '<?php sleep(1); echo "done";');

$fp = popen('PHP_CLI_SERVER_WORKERS=4 "' . $phl . '" -S localhost:' . $port
    . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(700000);

// fire two overlapping requests and time the pair
$start = microtime(true);
$cmd = 'curl -s http://localhost:' . $port . '/slow.php > /dev/null 2>&1 & '
     . 'curl -s http://localhost:' . $port . '/slow.php 2>/dev/null; wait';
$fp2 = popen($cmd, 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);
$elapsed = microtime(true) - $start;

echo trim($out), "\n";
echo $elapsed < 1.8 ? "concurrent" : "serialized ($elapsed)", "\n";

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(300000);
@unlink($tmpdir . '/slow.php');
@rmdir($tmpdir);
?>
--EXPECT--
done
concurrent
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $fp, $pid, $fp2, $out, $start, $elapsed, $cmd);
