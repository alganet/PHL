--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
https:// wrapper: the same exchange with the TLS transport under it
--DESCRIPTION--
php's https:// is its http:// wrapper opening `ssl://host:port` where the plain
one opens `tcp://`, and everything else about the exchange is unchanged: the
same request shape, the same `$http_response_header`, the same `http` label on
the handle's metadata, and the `ssl` context options going to the handshake.

What the SCHEME decides is three things, and each one is asked here: the port an
address with none defaults to (443, so the Host header of an https:// URL on a
non-default port still names it), whether TLS is spoken at all, and which scheme
a relative `Location:` is resolved against -- a redirect inside an https://
exchange stays on TLS rather than falling back to the plain wrapper.

The listener is a local one with a throwaway self-signed certificate, so the
context turns peer verification off; what is under test is the wrapper, not the
chain rules the ssl:// transport already owns.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test server";
} elseif (!extension_loaded('openssl') || !function_exists('openssl_csr_new')) {
    echo "skip needs openssl to mint the listener's certificate";
} elseif (!in_array('http', stream_get_wrappers(), true)) {
    echo "skip no http:// wrapper in this build";
}
?>
--FILE--
<?php
require __DIR__ . '/http_wrapper_server.inc';

var_dump(in_array('https', stream_get_wrappers(), true));

$dir = http_test_dir('tls');
$pem = $dir . DIRECTORY_SEPARATOR . 'server.pem';
if (!http_test_make_pem($pem)) {
    echo "no certificate\n";
    http_test_dir_clean($dir);
    return;
}
$port = http_test_server_start($proc, $dir, 19820, $pem);
if ($port === null) {
    echo "server did not start\n";
    http_test_dir_clean($dir);
    return;
}
$base = 'https://127.0.0.1:' . $port;
$ctx = stream_context_create(array('ssl' => array(
    'verify_peer' => false, 'verify_peer_name' => false)));

/* 1. a whole exchange over TLS, and the request the server was handed */
http_test_reply($dir, 'plain', "HTTP/1.1 200 OK\r\nX-Mark: one\r\n"
    . "Content-Length: 11\r\nConnection: close\r\n\r\nhello there");
$body = file_get_contents("$base/canned/plain", false, $ctx);
printf("-- body: %s\n", json_encode($body));
printf("-- headers: %s\n", json_encode($http_response_header));
$req = http_test_requests($dir);
printf("-- request: %s\n", json_encode(http_test_scrub($req[0])));
http_test_requests_clear($dir);

/* 2. the handle's own metadata: php labels the WRAPPER by its ops, which the
 *    two schemes share, and the stream by the transport under it */
$h = fopen("$base/canned/plain", 'r', false, $ctx);
$md = stream_get_meta_data($h);
printf("-- meta: %s\n", json_encode(array(
    'wrapper_type' => $md['wrapper_type'], 'stream_type' => $md['stream_type'],
    'seekable' => $md['seekable'], 'uri' => str_replace($base, 'BASE', $md['uri']),
    'wrapper_data' => $md['wrapper_data'],
)));
printf("-- read: %s\n", json_encode(stream_get_contents($h)));
fclose($h);
http_test_requests_clear($dir);

/* 3. a RELATIVE Location: inside an https:// exchange stays on https:// --
 *    the port is named because it is not the scheme's own */
http_test_reply($dir, 'hop', "HTTP/1.1 302 Found\r\nLocation: /canned/plain\r\n"
    . "Content-Length: 0\r\nConnection: close\r\n\r\n");
printf("-- redirected: %s\n", json_encode(file_get_contents("$base/canned/hop", false, $ctx)));
foreach (http_test_requests($dir) as $i => $raw) {
    printf("-- hop %d: %s\n", $i, json_encode(http_test_scrub($raw)));
}
http_test_requests_clear($dir);

/* 4. the http context options are the same set: this one is a header the
 *    server logs back */
$ctx2 = stream_context_create(array(
    'ssl' => array('verify_peer' => false, 'verify_peer_name' => false),
    'http' => array('method' => 'HEAD', 'header' => "X-Probe: yes\r\n",
        'user_agent' => 'phl-tls'),
));
@file_get_contents("$base/canned/plain", false, $ctx2);
$req = http_test_requests($dir);
printf("-- options: %s\n", json_encode(http_test_scrub($req[0])));

http_test_server_stop($proc, $port, true);
http_test_dir_clean($dir);
?>
--EXPECT--
bool(true)
-- body: "hello there"
-- headers: ["HTTP\/1.1 200 OK","X-Mark: one","Content-Length: 11","Connection: close"]
-- request: "GET \/canned\/plain HTTP\/1.1\nHost: HOST\nConnection: close\n\n"
-- meta: {"wrapper_type":"http","stream_type":"tcp_socket\/ssl","seekable":false,"uri":"BASE\/canned\/plain","wrapper_data":["HTTP\/1.1 200 OK","X-Mark: one","Content-Length: 11","Connection: close"]}
-- read: "hello there"
-- redirected: "hello there"
-- hop 0: "GET \/canned\/hop HTTP\/1.1\nHost: HOST\nConnection: close\n\n"
-- hop 1: "GET \/canned\/plain HTTP\/1.1\nHost: HOST\nConnection: close\n\n"
-- options: "HEAD \/canned\/plain HTTP\/1.1\nHost: HOST\nConnection: close\nUser-Agent: phl-tls\nX-Probe: yes\n\n"
