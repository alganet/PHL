--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
setcookie() in server mode: the attributes php actually puts on the wire
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
$tmpdir = sys_get_temp_dir() . '/phl_cookiehdr_' . getmypid();
require __DIR__ . '/free_port.inc';
$port = phpt_free_port(19850);
mkdir($tmpdir);
$script = '<?php
setcookie("plain", "v");
setcookie("full", "v", 2000000000, "/p", "ex.com", true, true);
setcookie("opts", "v", ["expires" => 2000000000, "path" => "/o", "domain" => "o.com",
    "secure" => true, "httponly" => true, "samesite" => "Lax"]);
setcookie("part", "v", ["partitioned" => true, "secure" => true, "samesite" => "None"]);
setcookie("gone", "");
setrawcookie("raw", "a+b");
setcookie("enc", "a b~c");
echo json_encode($_COOKIE), "\n";
';
file_put_contents($tmpdir . '/t.php', $script);

$fp = popen('"' . $phl . '" -S localhost:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(500000);

$fp2 = popen('curl -s -D - -H "Cookie: sent=x%20y; plus=p+q; tilde=%7Ez" --max-time 10 http://localhost:' . $port . '/t.php 2>/dev/null', 'r');
$out = '';
while (!feof($fp2)) { $out .= fgets($fp2); }
fclose($fp2);

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(200000);
unlink($tmpdir . '/t.php');
rmdir($tmpdir);

// Max-Age counts down from now, so it is the one field that cannot be pinned.
foreach (explode("\n", $out) as $line) {
    $line = rtrim($line, "\r");
    if (stripos($line, "Set-Cookie:") === 0) {
        echo preg_replace('/Max-Age=\d+/', 'Max-Age=N', $line), "\n";
    }
    // The body echoes $_COOKIE: what the browser sent comes back through the same
    // raw url-decoding setcookie() wrote it with, so a literal `+` stays a `+`.
    if (strncmp($line, '{"sent"', 7) === 0) {
        echo $line, "\n";
    }
}
?>
--EXPECT--
Set-Cookie: plain=v
Set-Cookie: full=v; expires=Wed, 18 May 2033 03:33:20 GMT; Max-Age=N; path=/p; domain=ex.com; secure; HttpOnly
Set-Cookie: opts=v; expires=Wed, 18 May 2033 03:33:20 GMT; Max-Age=N; path=/o; domain=o.com; secure; HttpOnly; SameSite=Lax
Set-Cookie: part=v; secure; SameSite=None; Partitioned
Set-Cookie: gone=deleted; expires=Thu, 01 Jan 1970 00:00:01 GMT; Max-Age=N
Set-Cookie: raw=a+b
Set-Cookie: enc=a%20b~c
{"sent":"x y","plus":"p+q","tilde":"~z"}
