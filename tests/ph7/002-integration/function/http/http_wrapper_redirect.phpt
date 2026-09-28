--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: the redirects it follows, and how it resolves them
--DESCRIPTION--
A 3xx carrying a `Location:` is followed unless `follow_location` says
otherwise, and the whole chain's headers accumulate in `$http_response_header`.

Resolving the target is php's own rule and not the URL RFC's: a location with a
scheme is taken whole, one starting with `/` replaces the path, and anything
else is joined to the CURRENT path up to and INCLUDING its last `/`, with
another `/` between -- so `/a/b` plus `rel` is `/a//rel`, unnormalized. A path
that is just `/`, or none at all, joins directly.

The method survives only the two codes that promise it: 307 and 308 re-send the
body, and 301/302/303 become a bodiless GET.

`max_redirects` counts REQUESTS rather than hops: following N redirects needs
N+1, so the default 20 allows nineteen. Running out is `Redirection limit
reached, aborting`, and it is a failed open like any other.
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

$dir = http_test_dir('rdr');
$port = http_test_server_start($proc, $dir);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'http://127.0.0.1:' . $port;

/** Arm one redirect, ask for $path, and report where the wrapper went next. */
function hop($label, $path, $loc, $code = 302, $opt = array(), $pin = true)
{
    global $dir, $base;
    http_test_reply($dir, 'go', "HTTP/1.1 $code Moved\r\nLocation: $loc\r\n"
        . "Content-Length: 0\r\nConnection: close\r\n\r\n");
    http_test_requests_clear($dir);
    $ctx = stream_context_create(array('http' => $opt));
    $r = @file_get_contents("$base$path", false, $ctx);
    $reqs = http_test_requests($dir);
    $second = count($reqs) > 1 ? http_test_scrub($reqs[1]) : '(no second request)';
    printf("-- %s\n", $label);
    if ($r === false) {
        printf("   FALSE %s\n", json_encode(substr(strstr(error_get_last()['message'], 'stream: '), 8)));
    }
    printf("   next: %s\n", $pin ? json_encode(strtok($second, "\n"))
        : ($second === '(no second request)' ? $second : '(a second request)'));
    unset($http_response_header);
}

/* The canned redirect lives at /canned/go, so the CURRENT path in every row
 * below is that one -- which is what a relative location is joined to. */
hop('an absolute path replaces the path', '/canned/go', '/target');
hop('a relative one is joined to the path, slash and all', '/canned/go', 'rel.txt');
hop('...with no normalization of the dots', '/canned/go', '../up.txt');
hop('a location with a scheme is taken whole', '/canned/go', "$base/whole");
hop('a protocol-relative one is just a path', '/canned/go', '//other.example/pr');
hop('a query rides along', '/canned/go', '/q?x=1');
hop('the location is trimmed', '/canned/go', '  /spaced  ');
hop('301 becomes a GET', '/canned/go', '/after', 301,
    array('method' => 'POST', 'content' => 'zz'));
hop('303 too', '/canned/go', '/after', 303,
    array('method' => 'POST', 'content' => 'zz'));
hop('307 keeps the method and the body', '/canned/go', '/after', 307,
    array('method' => 'POST', 'content' => 'zz'));
hop('308 as well', '/canned/go', '/after', 308,
    array('method' => 'PUT', 'content' => 'zz'));
hop('follow_location 0 stops at the 3xx', '/canned/go', '/after', 302,
    array('follow_location' => 0));
hop('max_redirects 1 refuses the first hop', '/canned/go', '/after', 302,
    array('max_redirects' => 1));
hop('max_redirects 2 allows it', '/canned/go', '/after', 302,
    array('max_redirects' => 2));

/* The whole chain's headers accumulate, in the order they arrived. */
http_test_reply($dir, 'a', "HTTP/1.1 302 Found\r\nLocation: /canned/b\r\n"
    . "X-Hop: one\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
http_test_reply($dir, 'b', "HTTP/1.1 302 Found\r\nLocation: /canned/c\r\n"
    . "X-Hop: two\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
http_test_reply($dir, 'c', "HTTP/1.1 200 OK\r\nX-Hop: three\r\n"
    . "Content-Length: 4\r\nConnection: close\r\n\r\ndone");
$r = @file_get_contents("$base/canned/a");
printf("-- a two-hop chain: %s\n   %s\n", json_encode($r), json_encode($http_response_header));
unset($http_response_header);

/* A Location of at most one byte is not joined at all: it goes under the root.
 * An empty one on a 3xx still moves, but only WHERE is php-version-dependent:
 * 8.5.11 goes to the root, and earlier builds decided by reading past the end
 * of the header -- so only the move is pinned. */
hop('a one-byte Location', '/canned/go', 'x');
hop('an empty Location', '/canned/go', '', 302, array(), false);

http_test_server_stop($proc, $port);
http_test_dir_clean($dir);
?>
--EXPECT--
-- an absolute path replaces the path
   next: "GET \/target HTTP\/1.1"
-- a relative one is joined to the path, slash and all
   next: "GET \/canned\/\/rel.txt HTTP\/1.1"
-- ...with no normalization of the dots
   next: "GET \/canned\/\/..\/up.txt HTTP\/1.1"
-- a location with a scheme is taken whole
   next: "GET \/whole HTTP\/1.1"
-- a protocol-relative one is just a path
   next: "GET \/\/other.example\/pr HTTP\/1.1"
-- a query rides along
   next: "GET \/q?x=1 HTTP\/1.1"
-- the location is trimmed
   next: "GET \/spaced HTTP\/1.1"
-- 301 becomes a GET
   next: "GET \/after HTTP\/1.1"
-- 303 too
   next: "GET \/after HTTP\/1.1"
-- 307 keeps the method and the body
   next: "POST \/after HTTP\/1.1"
-- 308 as well
   next: "PUT \/after HTTP\/1.1"
-- follow_location 0 stops at the 3xx
   next: "(no second request)"
-- max_redirects 1 refuses the first hop
   FALSE "Redirection limit reached, aborting"
   next: "(no second request)"
-- max_redirects 2 allows it
   next: "GET \/after HTTP\/1.1"
-- a two-hop chain: "done"
   ["HTTP\/1.1 302 Found","Location: \/canned\/b","X-Hop: one","Content-Length: 0","Connection: close","HTTP\/1.1 302 Found","Location: \/canned\/c","X-Hop: two","Content-Length: 0","Connection: close","HTTP\/1.1 200 OK","X-Hop: three","Content-Length: 4","Connection: close"]
-- a one-byte Location
   next: "GET \/x HTTP\/1.1"
-- an empty Location
   next: (a second request)
