--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_exec() runs a transfer, and CURLOPT_RETURNTRANSFER picks where the body goes
--DESCRIPTION--
The transfer verb, over the one protocol that needs no network: libcurl speaks
file://, so the whole output path -- the two destinations, the return value,
the error state, the byte counts -- is testable here without a listening
socket. The http:// half arrives with the local test server.

CURLOPT_RETURNTRANSFER is php's own option: no libcurl option carries that
number, and it does not reach the library at all. Every VALUE is accepted (the
setter always answers true) and only its truthiness matters, at exec time. With
it on, curl_exec answers the body as a string; with it off, the body goes to
the script's own OUTPUT -- which means the VM's output consumer, not the
process's stdout, so an ob_start() around the call captures it exactly as php
does. The return value is then true, not the body.

The failure paths answer false either way, and each leaves its own code: no URL
is CURLE_URL_MALFORMAT with php's "No URL set", an unknown scheme is
CURLE_UNSUPPORTED_PROTOCOL, and a missing file CURLE_FILE_COULDNT_READ_FILE. An
unreachable PORT is the one that is not pinned: POSIX refuses the connection
where Windows hangs until the timeout, so the test asserts that it failed to
connect rather than which of the two codes said so.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
function curlExecCase(string $label, callable $fn) {
    ob_start();
    try {
        $r = $fn();
        $out = ob_get_clean();
        echo $label, ' => ', var_export($r, true), ' printed=', var_export($out, true), "\n";
    } catch (Throwable $e) {
        ob_end_clean();
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

$h = curl_init();
curlExecCase('no url', fn() => curl_exec($h));
printf("   errno=%d error=%s\n", curl_errno($h), var_export(curl_error($h), true));

$u = curl_init('nonsense://x');
curlExecCase('bad scheme', fn() => curl_exec($u));
printf("   errno=%d\n", curl_errno($u));

$r = curl_init('http://127.0.0.1:1/');
curl_setopt($r, CURLOPT_TIMEOUT_MS, 200);
curlExecCase('refused, body to output', fn() => curl_exec($r));
// An unreachable port does not fail the same way everywhere: POSIX refuses the
// connection (CURLE_COULDNT_CONNECT) where Windows lets it hang until the
// timeout fires (CURLE_OPERATION_TIMEDOUT). Both are "it did not connect".
printf("   connect failed: %s\n", var_export(
    in_array(curl_errno($r), [CURLE_COULDNT_CONNECT, CURLE_OPERATION_TIMEDOUT], true), true));
curl_setopt($r, CURLOPT_RETURNTRANSFER, true);
curlExecCase('refused, body returned', fn() => curl_exec($r));

// RETURNTRANSFER takes any value and always answers true
$v = curl_init();
foreach ([true, 1, 'x', 0, false, null, []] as $val) {
    printf("RETURNTRANSFER %-8s => %s\n", var_export($val, true),
        var_export(curl_setopt($v, CURLOPT_RETURNTRANSFER, $val), true));
}

// a real transfer, no network involved
$tmp = tempnam(sys_get_temp_dir(), 'curlexec');
file_put_contents($tmp, "hello from a file\n");
// A file URL is not a path with a scheme glued on: Windows needs the drive
// letter behind a third slash and forward separators throughout.
$fileUrl = static function (string $path): string {
    return 'file://' . (DIRECTORY_SEPARATOR === '\\' ? '/' . str_replace('\\', '/', $path) : $path);
};
$url = $fileUrl($tmp);
$missing = $fileUrl(sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'nonexistent-curl-exec-probe');

$f = curl_init($url);
curl_setopt($f, CURLOPT_RETURNTRANSFER, true);
curlExecCase('file:// returned', fn() => curl_exec($f));
printf("   errno=%d http_code=%d size_download=%s\n", curl_errno($f),
    curl_getinfo($f, CURLINFO_RESPONSE_CODE), var_export(curl_getinfo($f, CURLINFO_SIZE_DOWNLOAD), true));

$f2 = curl_init($url);
curlExecCase('file:// to output', fn() => curl_exec($f2));

// the same handle twice: the second transfer answers the same bytes
$f3 = curl_init($url);
curl_setopt($f3, CURLOPT_RETURNTRANSFER, true);
$one = curl_exec($f3);
$two = curl_exec($f3);
var_dump($one === $two, curl_errno($f3));

// a success CLEARS the error a previous failure left
$f4 = curl_init($missing);
curl_setopt($f4, CURLOPT_RETURNTRANSFER, true);
curlExecCase('file:// missing', fn() => curl_exec($f4));
printf("   errno=%d\n", curl_errno($f4));
curl_setopt($f4, CURLOPT_URL, $url);
curl_exec($f4);
printf("   after a good transfer: errno=%d error=%s\n", curl_errno($f4), var_export(curl_error($f4), true));

unlink($tmp);
$rf = new ReflectionFunction('curl_exec');
echo 'curl_exec(', implode(', ', array_map(fn($p) => (string)$p->getType() . ' $' . $p->getName(), $rf->getParameters())),
    '): ', (string)$rf->getReturnType(), "\n";
--EXPECT--
no url => false printed=''
   errno=3 error='No URL set'
bad scheme => false printed=''
   errno=1
refused, body to output => false printed=''
   connect failed: true
refused, body returned => false printed=''
RETURNTRANSFER true     => true
RETURNTRANSFER 1        => true
RETURNTRANSFER 'x'      => true
RETURNTRANSFER 0        => true
RETURNTRANSFER false    => true
RETURNTRANSFER NULL     => true
RETURNTRANSFER array (
) => true
file:// returned => 'hello from a file
' printed=''
   errno=0 http_code=0 size_download=18.0
file:// to output => true printed='hello from a file
'
bool(true)
int(0)
file:// missing => false printed=''
   errno=37
   after a good transfer: errno=0 error=''
curl_exec(CurlHandle $handle): string|bool
