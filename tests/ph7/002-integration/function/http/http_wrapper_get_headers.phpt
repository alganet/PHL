--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_headers() and php 8.4's two last-response-header functions
--DESCRIPTION--
The three names a script asks an http:// exchange about.

`get_headers()` is the wrapper with `ignore_errors` forced ON -- a 404 is a set
of headers, not a failure -- opened and closed without a byte of the body read,
and it refuses anything php does not count as a URL wrapper -- a path, php://,
an unknown scheme -- while data://, which IS one, goes through and answers false
because a stream with no response headers has nothing to give. An empty url is
php's `Path must not be empty` before any of that.

php opens it for its HEADERS ONLY, which shows in two places: the dechunk filter
is never created, so a `Transfer-Encoding: chunked` header an ordinary read
consumes is still in the list, and running out of redirects stops the walk
SILENTLY where the same `max_redirects` on an ordinary read is a failed open.

`$associative` reshapes the SAME lines. A line with NO COLON in it is a status
line and takes the next INTEGER key -- which is how `abcdefghi 200 OK` gets one
and `HTTP/1.1 200 OK: weird` does not -- everything else is keyed by the text
before its first colon with the value left-trimmed after it, and a name that
arrives twice (across a redirect chain included) collects into an ARRAY.

`http_get_last_response_headers()` reads the same store `$http_response_header`
is written from, so it answers the last exchange's lines until
`http_clear_last_response_headers()` drops them -- and php drops them at the
START of every exchange too, so an open that never reached a response answers
NULL rather than the set before it.
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

$dir = http_test_dir('ghd');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;

http_test_reply($dir, 'plain', "HTTP/1.1 200 OK\r\nX-A: one\r\nX-A: two\r\nNoSpace:v\r\n"
    . ": empty\r\nX-Empty:\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
http_test_reply($dir, 'nostatuscolon', "abcdefghi 200 OK\r\nX-A: 1\r\n"
    . "Content-Length: 2\r\nConnection: close\r\n\r\nok");
http_test_reply($dir, 'statuscolon', "HTTP/1.1 200 OK: weird\r\nX-A: 1\r\n"
    . "Content-Length: 2\r\nConnection: close\r\n\r\nok");
http_test_reply($dir, 'fold', "HTTP/1.1 200 OK\r\nX-A: 1\r\n  cont\r\n"
    . "Content-Length: 2\r\nConnection: close\r\n\r\nok");
http_test_reply($dir, 'gone', "HTTP/1.1 404 Not Found\r\nX-B: b\r\n"
    . "Content-Length: 4\r\nConnection: close\r\n\r\nnope");
http_test_reply($dir, 'hop', "HTTP/1.1 302 Found\r\nLocation: /canned/plain\r\n"
    . "Content-Length: 0\r\nConnection: close\r\n\r\n");
http_test_reply($dir, 'chunked', "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
    . "X-A: 1\r\nConnection: close\r\n\r\n3\r\nabc\r\n0\r\n\r\n");

foreach (array('plain', 'nostatuscolon', 'statuscolon', 'fold', 'gone', 'hop', 'chunked') as $name) {
    printf("-- %s\n   flat : %s\n   assoc: %s\n", $name,
        json_encode(@get_headers("$base/canned/$name")),
        json_encode(@get_headers("$base/canned/$name", true)));
}

/* php opens this one for its HEADERS ONLY, which is two things: the dechunk
 * filter is never created, so the `Transfer-Encoding` header an ordinary read
 * consumes is STILL THERE, and running out of redirects is not a failure --
 * it stops where it is and answers what it has, silently, where the same
 * `max_redirects` on an ordinary read is `Redirection limit reached`. */
$seen = array();
set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
$capped = get_headers("$base/canned/hop", false, stream_context_create(array('http' => array('max_redirects' => 1))));
$read = file_get_contents("$base/canned/hop", false, stream_context_create(array('http' => array('max_redirects' => 1))));
restore_error_handler();
printf("-- capped get_headers: %s\n", json_encode($capped));
printf("-- capped read: %s / %s\n", var_export($read, true),
    json_encode(preg_replace('/\(.*?\)/', '(URL)', $seen)));
unset($http_response_header);

/* Not the HTTP wrapper: a path, an unknown scheme, and data://, which php
 * counts as a url wrapper and get_headers refuses all the same. */
foreach (array(__FILE__, '/nope/zz', 'zzz://x', 'data://text/plain,hi') as $what) {
    $seen = array();
    set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
    $r = get_headers($what);
    restore_error_handler();
    printf("-- not a URL: %s / %s\n", var_export($r, true), json_encode($seen));
}
try {
    get_headers('');
} catch (\Throwable $t) {
    printf("-- empty: %s: %s\n", get_class($t), $t->getMessage());
}
$dead = 'http://127.0.0.1:' . http_test_free_port(19980) . '/x';
$seen = array();
set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
$r = get_headers($dead);
restore_error_handler();
printf("-- refused: %s / %s\n", var_export($r, true),
    json_encode(preg_replace('/127\.0\.0\.1:\d+/', 'HOST', $seen)));

/* The store the two getters read, and the one $http_response_header comes from. */
http_clear_last_response_headers();
printf("-- cleared: %s\n", json_encode(http_get_last_response_headers()));
@file_get_contents("$base/canned/gone");
printf("-- after a 404 read: %s\n", json_encode(http_get_last_response_headers()));
@get_headers("$base/canned/fold");
printf("-- after get_headers: %s\n", json_encode(http_get_last_response_headers()));
@file_get_contents($dead);
printf("-- after an open that never answered: %s\n",
    json_encode(http_get_last_response_headers()));
printf('-- and $http_response_header kept its own: %s' . "\n",
    json_encode(isset($http_response_header) ? count($http_response_header) : 'unset'));

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
-- plain
   flat : ["HTTP\/1.1 200 OK","X-A: one","X-A: two","NoSpace:v",": empty","X-Empty:","Content-Length: 2","Connection: close"]
   assoc: {"0":"HTTP\/1.1 200 OK","X-A":["one","two"],"NoSpace":"v","":"empty","X-Empty":"","Content-Length":"2","Connection":"close"}
-- nostatuscolon
   flat : ["abcdefghi 200 OK","X-A: 1","Content-Length: 2","Connection: close"]
   assoc: {"0":"abcdefghi 200 OK","X-A":"1","Content-Length":"2","Connection":"close"}
-- statuscolon
   flat : ["HTTP\/1.1 200 OK: weird","X-A: 1","Content-Length: 2","Connection: close"]
   assoc: {"HTTP\/1.1 200 OK":"weird","X-A":"1","Content-Length":"2","Connection":"close"}
-- fold
   flat : ["HTTP\/1.1 200 OK","X-A: 1 cont","Content-Length: 2","Connection: close"]
   assoc: {"0":"HTTP\/1.1 200 OK","X-A":"1 cont","Content-Length":"2","Connection":"close"}
-- gone
   flat : ["HTTP\/1.1 404 Not Found","X-B: b","Content-Length: 4","Connection: close"]
   assoc: {"0":"HTTP\/1.1 404 Not Found","X-B":"b","Content-Length":"4","Connection":"close"}
-- hop
   flat : ["HTTP\/1.1 302 Found","Location: \/canned\/plain","Content-Length: 0","Connection: close","HTTP\/1.1 200 OK","X-A: one","X-A: two","NoSpace:v",": empty","X-Empty:","Content-Length: 2","Connection: close"]
   assoc: {"0":"HTTP\/1.1 302 Found","Location":"\/canned\/plain","Content-Length":["0","2"],"Connection":["close","close"],"1":"HTTP\/1.1 200 OK","X-A":["one","two"],"NoSpace":"v","":"empty","X-Empty":""}
-- chunked
   flat : ["HTTP\/1.1 200 OK","Transfer-Encoding: chunked","X-A: 1","Connection: close"]
   assoc: {"0":"HTTP\/1.1 200 OK","Transfer-Encoding":"chunked","X-A":"1","Connection":"close"}
-- capped get_headers: ["HTTP\/1.1 302 Found","Location: \/canned\/plain","Content-Length: 0","Connection: close"]
-- capped read: false / ["file_get_contents(URL): Failed to open stream: Redirection limit reached, aborting"]
-- not a URL: false / ["get_headers(): This function may only be used against URLs"]
-- not a URL: false / ["get_headers(): This function may only be used against URLs"]
-- not a URL: false / ["get_headers(): Unable to find the wrapper \"zzz\" - did you forget to enable it when you configured PHP?","get_headers(): This function may only be used against URLs"]
-- not a URL: false / []
-- empty: ValueError: Path must not be empty
-- refused: false / ["get_headers(http:\/\/HOST\/x): Failed to open stream: Connection refused"]
-- cleared: null
-- after a 404 read: ["HTTP\/1.1 404 Not Found","X-B: b","Content-Length: 4","Connection: close"]
-- after get_headers: ["HTTP\/1.1 200 OK","X-A: 1 cont","Content-Length: 2","Connection: close"]
-- after an open that never answered: null
-- and $http_response_header kept its own: 4
