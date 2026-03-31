--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Built-in server rejects headers containing newlines (CRLF injection prevention)
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
$port = 19050 + (getmypid() % 100);
mkdir($tmpdir);
$script = '<?php' . "\n"
    . 'error_reporting(E_ALL);' . "\n"
    . 'if (function_exists("ini_set")) { ini_set("display_errors", 1); ini_set("html_errors", 0); }' . "\n"
    . 'header("X-Good: safe");' . "\n"
    . 'header("X-Bad: foo\r\nX-Injected: bar");' . "\n"
    . 'echo "done";' . "\n";
file_put_contents($tmpdir . '/t.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s -D - http://localhost:' . $port . '/t.php 2>/dev/null', 'r');
$raw = '';
while (!feof($fp2)) { $raw .= fgets($fp2); }
fclose($fp2);

/* Split headers from body at the first blank line */
$parts = explode("\r\n\r\n", $raw, 2);
$headers = $parts[0];
$body = isset($parts[1]) ? $parts[1] : '';

$hasGood = strpos($headers, 'X-Good: safe') !== false;
$hasInjected = strpos($headers, 'X-Injected') !== false;
$hasWarning = strpos($body, 'new line detected') !== false;
$hasDone = strpos($body, 'done') !== false;
echo "good:" . ($hasGood ? "yes" : "no") . "\n";
echo "injected:" . ($hasInjected ? "yes" : "no") . "\n";
echo "warning:" . ($hasWarning ? "yes" : "no") . "\n";
echo "body:" . ($hasDone ? "yes" : "no");

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/t.php');
rmdir($tmpdir);
?>
--EXPECT--
good:yes
injected:no
warning:yes
body:yes
--CLEAN--
<?php
unset($phl, $tmpdir, $port, $script, $fp, $pid, $fp2, $raw, $parts, $headers, $body, $hasGood, $hasInjected, $hasWarning, $hasDone);
