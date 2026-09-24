--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The session cookie in server mode: built out of the session.cookie_* directives, and re-sent when the id changes
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
$tmpdir = sys_get_temp_dir() . '/phl_sesscookie_' . getmypid();
require __DIR__ . '/free_port.inc';
$port = phpt_free_port(19870);
mkdir($tmpdir);
mkdir($tmpdir . '/store');
$script = '<?php
ini_set("session.save_path", __DIR__ . "/store");
ini_set("session.cookie_path", "/app");
ini_set("session.cookie_domain", "example.test");
ini_set("session.cookie_secure", "1");
ini_set("session.cookie_httponly", "1");
ini_set("session.cookie_samesite", "Strict");
session_id("srv" . getmypid());
session_start();
session_regenerate_id();
';
file_put_contents($tmpdir . '/t.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s -D - -o /dev/null --max-time 10 http://localhost:' . $port . '/t.php 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
foreach (glob($tmpdir . '/store/*') as $f) { unlink($f); }
rmdir($tmpdir . '/store');
unlink($tmpdir . '/t.php');
rmdir($tmpdir);

// ONE cookie: sending the session cookie drops any earlier one of the same name,
// so a request that regenerates its id does not leave the old one in the reply.
foreach (explode("\n", $out) as $line) {
    $line = rtrim($line, "\r");
    if (stripos($line, "Set-Cookie: PHPSESSID=") === 0) {
        echo preg_replace('/PHPSESSID=[0-9a-f]{32}/', 'PHPSESSID=<fresh>', $line), "\n";
    }
}
?>
--EXPECT--
Set-Cookie: PHPSESSID=<fresh>; path=/app; domain=example.test; secure; HttpOnly; SameSite=Strict
