--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: fsockopen / stream_socket_client over a local HTTP server
--SKIPIF--
<?php
if (!defined('PH7_VERSION')) {
    // php's own -S is used as the peer under php; PHL serves with its own.
    // Both work, but the test needs the target executable to be able to serve.
}
// The peer server is backgrounded with POSIX shell (`>/dev/null 2>&1 & echo $!`)
// and the built-in `-S` server is POSIX-only; neither works on Windows (php fails
// this there too). The socket API itself is exercised by other tests.
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX -S server + shell harness'; }
?>
--FILE--
<?php
$bin = getenv('PHPT_TARGET_EXECUTABLE');
$tmpdir = sys_get_temp_dir() . '/phl_socktest_' . getmypid();
$port = 19600 + (getmypid() % 100);
mkdir($tmpdir);
file_put_contents($tmpdir . '/a.php', '<?php echo "sock-ok";');

$fp = popen('"' . $bin . '" -S 127.0.0.1:' . $port . ' -t "' . $tmpdir . '" >/dev/null 2>&1 & echo $!', 'r');
$pid = trim(fgets($fp));
fclose($fp);
usleep(700000);

// fsockopen: connect, speak HTTP/1.0, read the body back
$sock = fsockopen('tcp://127.0.0.1', $port, $errno, $errstr, 5);
echo is_resource($sock) ? 'resource' : 'FAILED', '|', $errno, '|', var_export($errstr, true), "\n";
fwrite($sock, "GET /a.php HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n");
$body = '';
while (!feof($sock)) {
    $body .= fread($sock, 4096);
}
fclose($sock);
echo substr($body, 0, 12), "\n";
echo str_contains($body, 'sock-ok') ? 'got-body' : 'no-body', "\n";

// stream_socket_client: same peer, address form
$c = stream_socket_client('tcp://127.0.0.1:' . $port, $e2, $s2, 5);
echo is_resource($c) ? 'resource' : 'FAILED', "\n";
fwrite($c, "GET /a.php HTTP/1.0\r\n\r\n");
echo str_contains(stream_get_contents($c), 'sock-ok') ? 'ssc-ok' : 'ssc-fail', "\n";
fclose($c);

// failure path: nothing listening — out-params carry the error
$bad = fsockopen('127.0.0.1', $port + 1, $e3, $s3, 1);
echo var_export($bad, true), '|', $e3 !== 0 ? 'errno-set' : 'errno-zero',
     '|', $s3 !== '' ? 'errstr-set' : 'errstr-empty', "\n";

popen('kill ' . $pid . ' 2>/dev/null', 'r');
usleep(300000);
@unlink($tmpdir . '/a.php');
@rmdir($tmpdir);
?>
--EXPECTF--
resource|0|''
HTTP/1.%d 200
got-body
resource
ssc-ok
%Afalse|errno-set|errstr-set
--CLEAN--
<?php
unset($bin, $tmpdir, $port, $fp, $pid, $sock, $errno, $errstr, $body, $c, $e2, $s2, $bad, $e3, $s3);
