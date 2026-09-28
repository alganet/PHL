--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a stream context's `notification` callback, over the http:// wrapper
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
/* The `notification` parameter has been accepted since stream_context_create()
 * became a real resource, and NOTHING called it: php notifies from http:// and
 * ftp://, and this build had neither. It has http:// now, so this is the whole
 * event stream php raises for one exchange. */
require __DIR__ . '/http_wrapper_server.inc';

$dir = http_test_dir('notify');
$port = http_test_server_start($proc, $dir, 19400);
if ($port === null) { echo "server did not start\n"; exit(1); }

$names = [];
foreach (get_defined_constants() as $k => $v) {
    if (strpos($k, 'STREAM_NOTIFY_') === 0 && strpos($k, 'SEVERITY') === false) {
        $names[$v] = substr($k, strlen('STREAM_NOTIFY_'));
    }
}
$log = [];
$cb = function ($code, $sev, $msg, $mcode, $bytes, $max) use (&$log, $names) {
    $log[] = sprintf('  %-13s sev=%d mcode=%d bytes=%d max=%d msg=%s',
        $names[$code] ?? "?$code", $sev, $mcode, $bytes, $max,
        $msg === null ? 'NULL' : var_export($msg, true));
};
$ctx = function ($cb, $opt = []) { return stream_context_create(['http' => $opt], ['notification' => $cb]); };
$show = function ($tag) use (&$log) { echo "== $tag\n"; foreach ($log as $l) { echo "$l\n"; } $log = []; };

/* (1) The ordinary exchange, in php's order: the CONNECTION, then the two
 * headers php reports, then the counter armed at zero and the body credited
 * as it arrives, then the end of the transfer. */
http_test_reply($dir, 'ok', "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nhi");
var_dump(file_get_contents("http://127.0.0.1:$port/canned/ok", false, $ctx($cb)));
$show('200');

/* (2) A status php will not open for is announced BEFORE the headers, with the
 * status LINE as it arrived and the code beside it -- and it is announced
 * whatever `ignore_errors` says, so a caller that reads the body anyway is
 * still told. */
http_test_reply($dir, 'e404', "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\nno!");
var_dump(@file_get_contents("http://127.0.0.1:$port/canned/e404", false, $ctx($cb)));
$show('404');
var_dump(file_get_contents("http://127.0.0.1:$port/canned/e404", false, $ctx($cb, ['ignore_errors' => true])));
$show('404 with ignore_errors');

/* (3) A redirect is a second CONNECTION, and php's wrapper follows one by
 * CALLING ITSELF -- so each frame arms the counter with the size IT announced
 * once the inner one has come back, innermost first. */
http_test_reply($dir, 'r1', "HTTP/1.1 302 Found\r\nContent-Length: 0\r\nLocation: /canned/ok\r\n\r\n");
var_dump(file_get_contents("http://127.0.0.1:$port/canned/r1", false, $ctx($cb)));
$show('302 -> 200');
/* The address is reported as the server WROTE it, and before the redirect
 * count is spent -- so the hop a limit refuses still says where it was going. */
var_dump(@file_get_contents("http://127.0.0.1:$port/canned/r1", false, $ctx($cb, ['max_redirects' => 1])));
$show('302 with max_redirects=1');
/* With following turned off there is no redirect to announce at all. */
var_dump(file_get_contents("http://127.0.0.1:$port/canned/r1", false, $ctx($cb, ['follow_location' => 0])));
$show('302 with follow_location=0');

/* (4) A CHUNKED body announces no size, and what is counted is what came out
 * of the dechunk filter rather than the framing that went in. Nothing reads
 * the socket to its end, so there is no completion either. */
http_test_reply($dir, 'ch', "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n2\r\n, \r\n5\r\nworld\r\n0\r\n\r\n");
var_dump(file_get_contents("http://127.0.0.1:$port/canned/ch", false, $ctx($cb)));
$show('chunked');

/* (5) A reply with no Content-Length at all: the counter runs with no maximum. */
http_test_reply($dir, 'nolen', "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nnolen");
var_dump(file_get_contents("http://127.0.0.1:$port/canned/nolen", false, $ctx($cb)));
$show('no Content-Length');

/* (6) WHICH `Content-Length` php announces: the value must be a plain run of
 * digits with the blanks after the colon skipped, so `+5`, `2x` and an empty
 * one are no announcement at all, `007` is seven, and a run too wide for the
 * clock saturates at its ceiling. The message is the whole LINE, where
 * `Content-Type` sends its VALUE. */
foreach (['007', '+5', '2x', '', '  4  ', '99999999999999999999'] as $i => $cl) {
    http_test_reply($dir, "cl$i", "HTTP/1.1 200 OK\r\nContent-Length: $cl\r\n\r\nabcd");
    file_get_contents("http://127.0.0.1:$port/canned/cl$i", false, $ctx($cb));
    $show("Content-Length: '$cl'");
}
http_test_reply($dir, 'sp', "HTTP/1.1 200 OK\r\nContent-Type:\ttext/plain ; charset=x  \r\nContent-Length: 4\r\n\r\nabcd");
file_get_contents("http://127.0.0.1:$port/canned/sp", false, $ctx($cb));
$show('odd spacing');
/* Two Content-Types are two announcements: php reports the LINE it is reading,
 * not the header it ends up with. */
http_test_reply($dir, 'two', "HTTP/1.1 200 OK\r\nContent-Type: a/b\r\nContent-Type: c/d\r\nContent-Length: 4\r\n\r\nabcd");
file_get_contents("http://127.0.0.1:$port/canned/two", false, $ctx($cb));
$show('two Content-Types');

/* (7) An informational response is DISCARDED whole -- the headers it carried
 * are never looked at, so a Content-Type on a 100 is not a mime type anybody
 * is told about. */
http_test_reply($dir, 'info',
    "HTTP/1.1 100 Continue\r\nContent-Type: junk/1xx\r\nContent-Length: 99\r\n\r\n"
  . "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nhi");
var_dump(file_get_contents("http://127.0.0.1:$port/canned/info", false, $ctx($cb)));
$show('100 then 200');

/* (8) The counter lives on the CONTEXT and php never disarms it, so a context
 * used a second time reports that exchange's request WRITE and its header READ
 * under the first exchange's running total -- until the wrapper arms it again. */
$reused = $ctx($cb);
file_get_contents("http://127.0.0.1:$port/canned/ok", false, $reused);
$log = [];
file_get_contents("http://127.0.0.1:$port/canned/ok", false, $reused);
$show('the same context, a second time');

/* (9) get_headers() is php's headers-only open: it arms the counter and
 * credits what arrived, and nothing reads the body, so nothing completes. */
var_dump(count(get_headers("http://127.0.0.1:$port/canned/ok", false, $ctx($cb))));
$show('get_headers');

/* (10) A connect that never happens notifies nothing at all. */
var_dump(@file_get_contents("http://127.0.0.1:1/nothing", false, $ctx($cb)));
$show('a refused connection');

/* (11) php stops asking once the callback has refused, and the refusal
 * unwinds through the open. */
$calls = 0;
$thrower = function ($code) use (&$calls) {
    $calls++;
    if ($calls === 2) { throw new RuntimeException('from the notifier'); }
};
try {
    file_get_contents("http://127.0.0.1:$port/canned/ok", false,
        stream_context_create([], ['notification' => $thrower]));
    echo "no throw\n";
} catch (Throwable $e) {
    echo "== caught ", get_class($e), ': ', $e->getMessage(), " after $calls calls\n";
}

/* (12) A callback is called with six arguments and may declare fewer. */
$few = [];
file_get_contents("http://127.0.0.1:$port/canned/ok", false,
    stream_context_create([], ['notification' => function ($c) use (&$few, $names) { $few[] = $names[$c]; }]));
echo "== two-of-six: ", implode(',', $few), "\n";

/* (13) And what is refused at the door: a `notification` that is not callable. */
try { stream_context_create([], ['notification' => 'no_such_function_at_all']); }
catch (Throwable $e) { echo '== ', get_class($e), ': ', $e->getMessage(), "\n"; }

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
string(2) "hi"
== 200
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=2 msg='Content-Length: 2'
  PROGRESS      sev=0 mcode=0 bytes=0 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=2 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=2 max=2 msg=NULL
bool(false)
== 404
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FAILURE       sev=2 mcode=404 bytes=0 max=0 msg='HTTP/1.1 404 Not Found
'
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=3 msg='Content-Length: 3'
string(3) "no!"
== 404 with ignore_errors
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FAILURE       sev=2 mcode=404 bytes=0 max=0 msg='HTTP/1.1 404 Not Found
'
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=3 msg='Content-Length: 3'
  PROGRESS      sev=0 mcode=0 bytes=0 max=3 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=3 max=3 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=3 max=3 msg=NULL
string(2) "hi"
== 302 -> 200
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=0 msg='Content-Length: 0'
  REDIRECTED    sev=0 mcode=0 bytes=0 max=0 msg='/canned/ok'
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=2 msg='Content-Length: 2'
  PROGRESS      sev=0 mcode=0 bytes=0 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=2 max=0 msg=NULL
bool(false)
== 302 with max_redirects=1
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=0 msg='Content-Length: 0'
  REDIRECTED    sev=0 mcode=0 bytes=0 max=0 msg='/canned/ok'
string(0) ""
== 302 with follow_location=0
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=0 msg='Content-Length: 0'
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=0 max=0 msg=NULL
string(12) "hello, world"
== chunked
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=12 max=0 msg=NULL
string(5) "nolen"
== no Content-Length
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=5 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=5 max=0 msg=NULL
== Content-Length: '007'
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=7 msg='Content-Length: 007'
  PROGRESS      sev=0 mcode=0 bytes=0 max=7 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=7 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=7 msg=NULL
== Content-Length: '+5'
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=0 msg=NULL
== Content-Length: '2x'
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=0 msg=NULL
== Content-Length: ''
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=0 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=0 msg=NULL
== Content-Length: '  4  '
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=4 msg='Content-Length:   4'
  PROGRESS      sev=0 mcode=0 bytes=0 max=4 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=4 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=4 msg=NULL
== Content-Length: '99999999999999999999'
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=9223372036854775807 msg='Content-Length: 99999999999999999999'
  PROGRESS      sev=0 mcode=0 bytes=0 max=9223372036854775807 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=9223372036854775807 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=9223372036854775807 msg=NULL
== odd spacing
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain ; charset=x'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=4 msg='Content-Length: 4'
  PROGRESS      sev=0 mcode=0 bytes=0 max=4 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=4 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=4 msg=NULL
== two Content-Types
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='a/b'
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='c/d'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=4 msg='Content-Length: 4'
  PROGRESS      sev=0 mcode=0 bytes=0 max=4 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=4 max=4 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=4 max=4 msg=NULL
string(2) "hi"
== 100 then 200
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=2 msg='Content-Length: 2'
  PROGRESS      sev=0 mcode=0 bytes=0 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=2 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=2 max=2 msg=NULL
== the same context, a second time
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=71 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=137 max=2 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=2 msg='Content-Length: 2'
  PROGRESS      sev=0 mcode=0 bytes=0 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=2 msg=NULL
  COMPLETED     sev=0 mcode=0 bytes=2 max=2 msg=NULL
int(3)
== get_headers
  CONNECT       sev=0 mcode=0 bytes=0 max=0 msg=NULL
  MIME_TYPE_IS  sev=0 mcode=0 bytes=0 max=0 msg='text/plain'
  FILE_SIZE_IS  sev=0 mcode=0 bytes=0 max=2 msg='Content-Length: 2'
  PROGRESS      sev=0 mcode=0 bytes=0 max=2 msg=NULL
  PROGRESS      sev=0 mcode=0 bytes=2 max=2 msg=NULL
bool(false)
== a refused connection
== caught RuntimeException: from the notifier after 2 calls
== two-of-six: CONNECT,MIME_TYPE_IS,FILE_SIZE_IS,PROGRESS,PROGRESS,COMPLETED
== TypeError: stream_context_create(): Argument #1 ($options) must be an array with valid callbacks as values, function "no_such_function_at_all" not found or invalid function name
