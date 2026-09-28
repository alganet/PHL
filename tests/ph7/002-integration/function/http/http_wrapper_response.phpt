--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: what it makes of a response
--DESCRIPTION--
The reading half, pinned against replies a real server would rarely produce --
which is the point, because php's rules here are not the RFC's:

  * THE STATUS CODE IS `atoi(line + 9)`, with no prefix check at all. So
    `HTTP/2.0 200 OK` is a 200, `HTTP/2 200 OK` is a 0 (its digits sit at the
    wrong offset), a nine-byte line has no code, and a line that is not HTTP at
    all is read the same way.
  * ANYTHING OUTSIDE 200..399 FAILS the open, with `HTTP request failed! ` and
    the status line AS READ -- terminator included, which is why a reply that
    simply ran out of bytes is named with no newline after it. `ignore_errors`
    turns the failure into an ordinary read.
  * A 1xx IS DISCARDED, headers and all, and the next response read in its
    place -- except 101, which php reports as itself.
  * THE BODY ENDS WHERE THE SOCKET DOES. `Content-Length` is recorded and never
    enforced: a header promising 100 bytes of a five-byte body answers the five,
    and one promising 2 of fifteen answers the fifteen.
  * `Transfer-Encoding: chunked` IS the one framing php applies, and its header
    line is then not part of the response headers at all. Chunk extensions and
    the trailer after the final chunk are consumed and never seen.
  * A CONTINUATION LINE joins the header before it with a single space, and a
    header line with NO COLON refuses the whole response.

`$http_response_header` is written into the calling scope for every one of
these, the failures included: a 404 is a failed open with a complete set of
headers behind it.
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

$dir = http_test_dir('rsp');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;

function reply($label, $bytes, $opt = array())
{
    global $dir, $base;
    http_test_reply($dir, 'r', $bytes);
    $ctx = stream_context_create(array('http' => $opt));
    $r = @file_get_contents("$base/canned/r", false, $ctx);
    printf("-- %s\n   body: %s\n", $label, $r === false
        ? 'FALSE ' . json_encode(substr(strstr(error_get_last()['message'], 'stream: '), 8))
        : json_encode($r));
    printf("   hdrs: %s\n", json_encode(isset($http_response_header) ? $http_response_header : null));
    unset($http_response_header);
}

reply('an ordinary 200',
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('HTTP/2.0 is a 200 to atoi(line+9)',
    "HTTP/2.0 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('HTTP/2 puts its digits at the wrong offset',
    "HTTP/2 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a line under ten bytes has no code',
    "HTTP/1.1\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('no prefix is checked either',
    "abcdefghi 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a 404 fails the open',
    "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\nConnection: close\r\n\r\nnope");
reply('...and ignore_errors reads it',
    "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\nConnection: close\r\n\r\nnope",
    array('ignore_errors' => true));
reply('a 3xx with no Location is an ordinary answer',
    "HTTP/1.1 302 Found\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a 204 has no body and no length',
    "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
reply('a 100 is discarded, headers and all',
    "HTTP/1.1 100 Continue\r\nX-Gone: 1\r\n\r\nHTTP/1.1 200 OK\r\nX-Real: 1\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a 199 too',
    "HTTP/1.1 199 Odd\r\nX-Gone: 1\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('101 is the exception php reports as itself',
    "HTTP/1.1 101 Switching\r\nX-Kept: 1\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('Content-Length under-promises and is ignored',
    "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nConnection: close\r\n\r\nshort");
reply('...and over-promises just the same',
    "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nlonger-than-two");
reply('a chunked body, its extension and its trailer',
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: text/x\r\nConnection: close\r\n\r\n"
    . "3;ext=1\r\nabc\r\n2\r\nde\r\n0\r\nX-Trail: 1\r\n\r\n");
reply('an identity Transfer-Encoding is just a header',
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: identity\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a continuation line joins with one space',
    "HTTP/1.1 200 OK\r\nX-Fold: one\r\n  two\r\n\tthree\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a repeated header keeps both',
    "HTTP/1.1 200 OK\r\nX-D: a\r\nX-D: b\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a header with an empty NAME is still a header',
    "HTTP/1.1 200 OK\r\n: v\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('a header line with no colon refuses the response',
    "HTTP/1.1 200 OK\r\nJustAWord\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
reply('bare LF line endings are read too',
    "HTTP/1.1 200 OK\nX-A: 1\nContent-Length: 2\nConnection: close\n\nok");
reply('headers that simply stop', "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n");
reply('a status line and nothing else', "HTTP/1.1 200 OK\r\n\r\nbody-here");
reply('bytes that are not a reply at all', "raw bytes only, no headers");
reply('nothing at all', '');
reply('a bodiless 1xx leaves php reporting its blank line',
    "HTTP/1.1 100 Info\r\nX-One: v\r\n\r\n");

/* An open the wrapper never got an answer to leaves the caller's own
 * $http_response_header alone -- there is nothing to publish. */
$http_response_header = array('kept');
$dead = 'http://127.0.0.1:' . http_test_free_port(19990) . '/x';
printf("-- an open that never reached a response: %s / %s\n",
    var_export(@file_get_contents($dead), true), json_encode($http_response_header));

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
-- an ordinary 200
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","Content-Type: text\/plain","Content-Length: 2","Connection: close"]
-- HTTP/2.0 is a 200 to atoi(line+9)
   body: "ok"
   hdrs: ["HTTP\/2.0 200 OK","Content-Length: 2","Connection: close"]
-- HTTP/2 puts its digits at the wrong offset
   body: FALSE "HTTP request failed! HTTP\/2 200 OK\r\n"
   hdrs: ["HTTP\/2 200 OK","Content-Length: 2","Connection: close"]
-- a line under ten bytes has no code
   body: FALSE "HTTP request failed! HTTP\/1.1\r\n"
   hdrs: ["HTTP\/1.1","Content-Length: 2","Connection: close"]
-- no prefix is checked either
   body: "ok"
   hdrs: ["abcdefghi 200 OK","Content-Length: 2","Connection: close"]
-- a 404 fails the open
   body: FALSE "HTTP request failed! HTTP\/1.1 404 Not Found\r\n"
   hdrs: ["HTTP\/1.1 404 Not Found","Content-Length: 4","Connection: close"]
-- ...and ignore_errors reads it
   body: "nope"
   hdrs: ["HTTP\/1.1 404 Not Found","Content-Length: 4","Connection: close"]
-- a 3xx with no Location is an ordinary answer
   body: "ok"
   hdrs: ["HTTP\/1.1 302 Found","Content-Length: 2","Connection: close"]
-- a 204 has no body and no length
   body: ""
   hdrs: ["HTTP\/1.1 204 No Content","Connection: close"]
-- a 100 is discarded, headers and all
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","X-Real: 1","Content-Length: 2","Connection: close"]
-- a 199 too
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","Content-Length: 2","Connection: close"]
-- 101 is the exception php reports as itself
   body: FALSE "HTTP request failed! HTTP\/1.1 101 Switching\r\n"
   hdrs: ["HTTP\/1.1 101 Switching","X-Kept: 1","Content-Length: 2","Connection: close"]
-- Content-Length under-promises and is ignored
   body: "short"
   hdrs: ["HTTP\/1.1 200 OK","Content-Length: 100","Connection: close"]
-- ...and over-promises just the same
   body: "longer-than-two"
   hdrs: ["HTTP\/1.1 200 OK","Content-Length: 2","Connection: close"]
-- a chunked body, its extension and its trailer
   body: "abcde"
   hdrs: ["HTTP\/1.1 200 OK","Content-Type: text\/x","Connection: close"]
-- an identity Transfer-Encoding is just a header
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","Transfer-Encoding: identity","Content-Length: 2","Connection: close"]
-- a continuation line joins with one space
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","X-Fold: one two three","Content-Length: 2","Connection: close"]
-- a repeated header keeps both
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","X-D: a","X-D: b","Content-Length: 2","Connection: close"]
-- a header with an empty NAME is still a header
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK",": v","Content-Length: 2","Connection: close"]
-- a header line with no colon refuses the response
   body: FALSE "HTTP invalid response format (no colon in header line)!"
   hdrs: ["HTTP\/1.1 200 OK"]
-- bare LF line endings are read too
   body: "ok"
   hdrs: ["HTTP\/1.1 200 OK","X-A: 1","Content-Length: 2","Connection: close"]
-- headers that simply stop
   body: ""
   hdrs: ["HTTP\/1.1 200 OK","Content-Length: 2"]
-- a status line and nothing else
   body: "body-here"
   hdrs: ["HTTP\/1.1 200 OK"]
-- bytes that are not a reply at all
   body: FALSE "HTTP request failed! raw bytes only, no headers"
   hdrs: ["raw bytes only, no headers"]
-- nothing at all
   body: FALSE "HTTP request failed!"
   hdrs: []
-- a bodiless 1xx leaves php reporting its blank line
   body: FALSE "HTTP request failed! \r\n"
   hdrs: [""]
-- an open that never reached a response: false / ["kept"]
