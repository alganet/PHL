--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The cache headers a session puts on its reply, one per session.cache_limiter
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
$tmpdir = sys_get_temp_dir() . '/phl_sesscache_' . getmypid();
require __DIR__ . '/free_port.inc';
$port = phpt_free_port(19890);
mkdir($tmpdir);
mkdir($tmpdir . '/store');
$script = '<?php
ini_set("session.save_path", __DIR__ . "/store");
ini_set("session.cache_limiter", $_GET["l"]);
ini_set("session.cache_expire", "180");
session_start();
';
file_put_contents($tmpdir . '/t.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

foreach (["nocache", "public", "private", "private_no_expire", "bogus"] as $limiter) {
    $fp2 = popen('curl -s -D - -o /dev/null --max-time 10 "http://localhost:' . $port
        . '/t.php?l=' . $limiter . '" 2>/dev/null', 'r');
    $out = '';
    while (!feof($fp2)) { $out .= fgets($fp2); }
    fclose($fp2);
    echo "-- ", $limiter, "\n";
    foreach (explode("\n", $out) as $line) {
        $line = rtrim($line, "\r");
        // Expires under `public` and Last-Modified are both clock-dependent; only
        // php's already-expired constant is a fixed date.
        if (preg_match('/^(Expires|Cache-Control|Pragma|Last-Modified):/i', $line)) {
            if (preg_match('/^(Last-Modified|Expires):/i', $line)
                && stripos($line, "Expires: Thu, 19 Nov 1981") !== 0) {
                $line = preg_replace('/: .*$/', ': <date>', $line);
            }
            echo $line, "\n";
        }
    }
}

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
foreach (glob($tmpdir . '/store/*') as $f) { unlink($f); }
rmdir($tmpdir . '/store');
unlink($tmpdir . '/t.php');
rmdir($tmpdir);
?>
--EXPECT--
-- nocache
Expires: Thu, 19 Nov 1981 08:52:00 GMT
Cache-Control: no-store, no-cache, must-revalidate
Pragma: no-cache
-- public
Expires: <date>
Cache-Control: public, max-age=10800
Last-Modified: <date>
-- private
Expires: Thu, 19 Nov 1981 08:52:00 GMT
Cache-Control: private, max-age=10800
Last-Modified: <date>
-- private_no_expire
Cache-Control: private, max-age=10800
Last-Modified: <date>
-- bogus
