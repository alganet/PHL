--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: the request it composes, slot by slot
--DESCRIPTION--
What the http:// wrapper puts on the wire is a FIXED slot order that the `http`
context options and two ini directives fill in, and a script-supplied header
wins over the slot it names. The server here answers the request bytes back, so
every assertion is about what was really written.

The slots, in order: the request line (whose protocol version is a DOUBLE php
prints with one decimal, so `'2.0'` is 2.0 and `null` is 0.0), the URL's own
credentials as
`Authorization: Basic`, the `from` ini, `Host` (which drops a `:80` and keeps
every other port), `Connection: close`, `User-Agent`, the automatic
`Content-Length`, the script's own headers, and last the automatic
`Content-Type`. A `Host:`, `Connection:`, `User-Agent:`, `Authorization:`,
`Content-Length:` or `Content-Type:` of one's own suppresses the slot it names.

Two shapes of `header` are read -- one string whose line breaks separate the
headers, and an ARRAY of them, whose non-string entries are dropped and whose
rest is joined with CRLF. php then trims the whole block at both ends and emits
what is left verbatim, so a blank line in the MIDDLE ends the request's header
block and everything after it becomes the request body. A `content` that is the
empty string adds neither length nor type.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!in_array('http', stream_get_wrappers(), true)) {
    echo "skip no http:// wrapper in this build";
}
?>
--FILE--
<?php
require __DIR__ . '/http_wrapper_server.inc';

$dir = http_test_dir('req');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;

function show($label, $url, $opt)
{
    global $dir;
    http_test_requests_clear($dir);
    $ctx = stream_context_create(array('http' => $opt));
    $seen = array();
    set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
    $r = file_get_contents($url, false, $ctx);
    restore_error_handler();
    echo "-- $label\n";
    foreach ($seen as $line) {
        echo '   ! ', $line, "\n";
    }
    if ($r === false) {
        echo "FALSE\n";
        return;
    }
    $reqs = http_test_requests($dir);
    echo http_test_scrub($reqs[0]), "|\n";
}

show('plain GET', "$base/echo", array());
show('protocol_version 1.0', "$base/echo", array('protocol_version' => '1.0'));
show('protocol_version as a float', "$base/echo", array('protocol_version' => 1.1));
show('...and as a number php prints with one decimal', "$base/echo",
    array('protocol_version' => '1.15'));
show('...so a non-numeric one is 0.0', "$base/echo", array('protocol_version' => 'abc'));
show('...and null is too', "$base/echo", array('protocol_version' => null));
show('method and content', "$base/echo", array('method' => 'POST', 'content' => 'a=1&b=2'));
show('empty content adds nothing', "$base/echo", array('method' => 'POST', 'content' => ''));
show('user_agent option', "$base/echo", array('user_agent' => 'probe/1'));
show('header as one string', "$base/echo", array('header' => "X-A: 1\r\nX-B: 2"));
show('header as an array', "$base/echo", array('header' => array('X-C: 3', 5, 'X-D: 4')));
show('a blank line ENDS the block', "$base/echo", array('header' => array('X-C: 3', '', 'X-D: 4')));
show('blank ends and edges are trimmed', "$base/echo", array('header' => array('   ', "\tX-E: 5", '')));
show('own Host wins', "$base/echo", array('header' => 'Host: elsewhere.example'));
show('own Connection wins', "$base/echo", array('header' => 'Connection: keep-alive'));
show('own User-Agent beats the option', "$base/echo",
    array('user_agent' => 'ignored/9', 'header' => 'user-agent: mine/2'));
show('own Content-Type and Length', "$base/echo",
    array('method' => 'PUT', 'content' => 'abcdef', 'header' => "Content-Type: text/plain\r\nContent-Length: 3"));
show('URL credentials', "http://user:pw@127.0.0.1:$port/echo", array());
show('own Authorization wins', "http://user:pw@127.0.0.1:$port/echo",
    array('header' => 'Authorization: Bearer opaque'));
show('query survives, fragment does not', "$base/echo?a=1&b=2#frag", array());
show('no path at all', "http://127.0.0.1:$port", array());
show('request_fulluri', "$base/echo", array('request_fulluri' => true));

/* A proxy moves the CONNECTION and leaves the request describing the origin. */
show('proxy', 'http://origin.example/echo', array('proxy' => "tcp://127.0.0.1:$port"));

/* The `from` ini writes a header whenever it has a VALUE -- an empty one still
 * does -- while an empty user_agent writes nothing at all. */
ini_set('from', 'me@example.org');
ini_set('user_agent', 'IniUA/3');
show('the two ini directives', "$base/echo", array());
ini_set('user_agent', '');
show('an empty user_agent writes no header', "$base/echo", array());
ini_set('from', '');
http_test_requests_clear($dir);
@file_get_contents("$base/echo");
$req = http_test_requests($dir);
printf("-- an empty from still writes the header: %s\n",
    var_export(strpos($req[0], "From: \r\n") !== false, true));

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
-- plain GET
GET /echo HTTP/1.1
Host: HOST
Connection: close

|
-- protocol_version 1.0
GET /echo HTTP/1.0
Host: HOST
Connection: close

|
-- protocol_version as a float
GET /echo HTTP/1.1
Host: HOST
Connection: close

|
-- ...and as a number php prints with one decimal
GET /echo HTTP/1.1
Host: HOST
Connection: close

|
-- ...so a non-numeric one is 0.0
GET /echo HTTP/0.0
Host: HOST
Connection: close

|
-- ...and null is too
GET /echo HTTP/0.0
Host: HOST
Connection: close

|
-- method and content
   ! file_get_contents(): Content-type not specified assuming application/x-www-form-urlencoded
POST /echo HTTP/1.1
Host: HOST
Connection: close
Content-Length: 7
Content-Type: application/x-www-form-urlencoded

a=1&b=2|
-- empty content adds nothing
POST /echo HTTP/1.1
Host: HOST
Connection: close

|
-- user_agent option
GET /echo HTTP/1.1
Host: HOST
Connection: close
User-Agent: probe/1

|
-- header as one string
GET /echo HTTP/1.1
Host: HOST
Connection: close
X-A: 1
X-B: 2

|
-- header as an array
GET /echo HTTP/1.1
Host: HOST
Connection: close
X-C: 3
X-D: 4

|
-- a blank line ENDS the block
GET /echo HTTP/1.1
Host: HOST
Connection: close
X-C: 3

|
-- blank ends and edges are trimmed
GET /echo HTTP/1.1
Host: HOST
Connection: close
X-E: 5

|
-- own Host wins
GET /echo HTTP/1.1
Connection: close
Host: elsewhere.example

|
-- own Connection wins
GET /echo HTTP/1.1
Host: HOST
Connection: keep-alive

|
-- own User-Agent beats the option
GET /echo HTTP/1.1
Host: HOST
Connection: close
user-agent: mine/2

|
-- own Content-Type and Length
PUT /echo HTTP/1.1
Host: HOST
Connection: close
Content-Type: text/plain
Content-Length: 3

abc|
-- URL credentials
GET /echo HTTP/1.1
Authorization: Basic dXNlcjpwdw==
Host: HOST
Connection: close

|
-- own Authorization wins
GET /echo HTTP/1.1
Host: HOST
Connection: close
Authorization: Bearer opaque

|
-- query survives, fragment does not
GET /echo?a=1&b=2 HTTP/1.1
Host: HOST
Connection: close

|
-- no path at all
GET / HTTP/1.1
Host: HOST
Connection: close

|
-- request_fulluri
GET http://HOST/echo HTTP/1.1
Host: HOST
Connection: close

|
-- proxy
GET /echo HTTP/1.1
Host: origin.example
Connection: close

|
-- the two ini directives
GET /echo HTTP/1.1
From: me@example.org
Host: HOST
Connection: close
User-Agent: IniUA/3

|
-- an empty user_agent writes no header
GET /echo HTTP/1.1
From: me@example.org
Host: HOST
Connection: close

|
-- an empty from still writes the header: true
