--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
CURLOPT_POSTFIELDS, the string form: the body a POST carries
--DESCRIPTION--
The single most common thing a program does with this extension after fetching
a URL, and until now a loud refusal. php's rule for a non-array value is the
ordinary string cast -- an int, a float, a bool, null and a resource all have a
spelling -- and then the LENGTH beside the bytes, which is the whole difference
from a plain string option: a body may contain a NUL and php does not screen
for one.

Setting it makes the request a POST and gives it libcurl's default
`Content-Type: application/x-www-form-urlencoded`, which nothing in php chooses
-- the library does, and a caller who wants another one sends its own header.
`false` and `null` cast to the empty string, so they are a POST with a
zero-length body rather than "no body": the request is still a POST.

libcurl COPIES the bytes, so the caller's string may die the moment setopt
returns and a copied handle carries the body with it. A later CURLOPT_HTTPGET
takes the body off again -- that is the library's own precedence and not php's
-- and curl_reset() takes it off with everything else.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
}
?>
--FILE--
<?php
require __DIR__ . '/http_server.inc';

class CurlPostStringable
{
    public function __toString(): string
    {
        return 'from=__toString';
    }
}
class CurlPostPlain
{
}

$port = curl_test_server_start($proc, 19680);
if ($port === null) {
    echo "server did not start\n";
    return;
}
$base = 'http://127.0.0.1:' . $port;

$post = static function ($value) use ($base) {
    $h = curl_init($base . '/echo');
    curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($h, CURLOPT_TIMEOUT, 10);
    try {
        $set = var_export(curl_setopt($h, CURLOPT_POSTFIELDS, $value), true);
    } catch (Throwable $e) {
        $set = get_class($e) . ': ' . $e->getMessage();
    }
    $raw = curl_test_request(curl_exec($h));
    if ($raw === false) {
        return $set . ' exec=false errno=' . curl_errno($h);
    }
    $split = strpos($raw, "\r\n\r\n");
    $head = substr($raw, 0, $split);
    $body = substr($raw, $split + 4);
    $line = array();
    foreach (explode("\r\n", $head) as $l) {
        if (strncmp($l, 'Host:', 5) !== 0 && strncmp($l, 'Accept:', 7) !== 0) {
            $line[] = $l;
        }
    }
    return $set . ' | ' . implode(' | ', $line) . ' | body=' . str_replace("\0", '<NUL>', $body);
};

foreach (array(
    'int' => 42,
    'float' => 1.5,
    'true' => true,
    'false' => false,
    'null' => null,
    'string' => 'a=1&b=2',
    'empty' => '',
    'with a NUL' => "a\0b",
    'stringable' => new CurlPostStringable(),
    'no __toString' => new CurlPostPlain(),
) as $label => $value) {
    printf("%-14s %s\n", $label, $post($value));
}

/* libcurl copies the body: the source may go, and a copy carries it */
$h = curl_init($base . '/echo');
curl_setopt($h, CURLOPT_RETURNTRANSFER, true);
curl_setopt($h, CURLOPT_TIMEOUT, 10);
$body = 'kept=yes';
curl_setopt($h, CURLOPT_POSTFIELDS, $body);
unset($body);
$copy = curl_copy_handle($h);
printf("source string gone: %s\n", var_export(substr(strrchr(curl_test_request(curl_exec($h)), "\n"), 1), true));
printf("copy carries it:    %s\n", var_export(substr(strrchr(curl_test_request(curl_exec($copy)), "\n"), 1), true));

/* CURLOPT_HTTPGET afterwards takes the body off */
curl_setopt($h, CURLOPT_HTTPGET, 1);
$raw = curl_test_request(curl_exec($h));
printf("after HTTPGET:      %s\n", var_export(strtok($raw, "\r"), true));

/* and so does curl_reset() */
$r = curl_init($base . '/echo');
curl_setopt($r, CURLOPT_POSTFIELDS, 'gone=soon');
curl_reset($r);
curl_setopt($r, CURLOPT_URL, $base . '/echo');
curl_setopt($r, CURLOPT_RETURNTRANSFER, true);
curl_setopt($r, CURLOPT_TIMEOUT, 10);
printf("after curl_reset:   %s\n", var_export(strtok(curl_test_request(curl_exec($r)), "\r"), true));

curl_test_server_stop($proc, $port);
?>
--EXPECT--
int            true | POST /echo HTTP/1.1 | Content-Length: 2 | Content-Type: application/x-www-form-urlencoded | body=42
float          true | POST /echo HTTP/1.1 | Content-Length: 3 | Content-Type: application/x-www-form-urlencoded | body=1.5
true           true | POST /echo HTTP/1.1 | Content-Length: 1 | Content-Type: application/x-www-form-urlencoded | body=1
false          true | POST /echo HTTP/1.1 | Content-Length: 0 | Content-Type: application/x-www-form-urlencoded | body=
null           true | POST /echo HTTP/1.1 | Content-Length: 0 | Content-Type: application/x-www-form-urlencoded | body=
string         true | POST /echo HTTP/1.1 | Content-Length: 7 | Content-Type: application/x-www-form-urlencoded | body=a=1&b=2
empty          true | POST /echo HTTP/1.1 | Content-Length: 0 | Content-Type: application/x-www-form-urlencoded | body=
with a NUL     true | POST /echo HTTP/1.1 | Content-Length: 3 | Content-Type: application/x-www-form-urlencoded | body=a<NUL>b
stringable     true | POST /echo HTTP/1.1 | Content-Length: 15 | Content-Type: application/x-www-form-urlencoded | body=from=__toString
no __toString  Error: Object of class CurlPostPlain could not be converted to string | POST /echo HTTP/1.1 | Content-Length: 0 | Content-Type: application/x-www-form-urlencoded | body=
source string gone: 'kept=yes'
copy carries it:    'kept=yes'
after HTTPGET:      'GET /echo HTTP/1.1'
after curl_reset:   'GET /echo HTTP/1.1'
--CLEAN--
<?php
unset($port, $base, $proc, $post, $h, $copy, $raw, $r, $label, $value, $body);
?>
