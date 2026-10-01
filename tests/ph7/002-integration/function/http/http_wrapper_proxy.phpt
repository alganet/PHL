--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http:// wrapper: an https:// origin reached through a proxy
--DESCRIPTION--
The `proxy` context option moves the CONNECTION and leaves the request alone --
but a TLS origin cannot be spoken to through a proxy that reads the request, so
php TUNNELS: it writes a `CONNECT host:port HTTP/1.0` to the proxy, reads its
answer off, and runs the handshake inside what is left. The origin's own name,
not the proxy's, is what the certificate is checked against.

The CONNECT is the shortest request the wrapper ever writes. No Host, no
User-Agent, no `http` context option of any kind reaches it, and the version is
a fixed 1.0 whatever the exchange behind the tunnel will use. Its ONE optional
line is a `Proxy-Authorization:` lifted out of the script's own `header` --
which is then taken back OUT of the request behind the tunnel, because that
credential addressed the proxy and the tunnelled request does not.

Everything else about the exchange is unchanged: the request line still names
the origin's path (or the whole URL under `request_fulluri`), and the Host
header still names the origin and its port.

php looks at NEITHER the status line nor the headers the proxy answers with, so
a proxy that refuses is told apart from one that agreed only by the handshake
behind it failing -- and that failure is reported as one fixed sentence naming
the proxy rather than as the TLS error under it. A tunnel that never came up
notifies nothing: `STREAM_NOTIFY_CONNECT` reports a connection that was MADE.

A plain http:// origin through the same proxy tunnels nothing: the request goes
straight to the proxy, which is the whole of what the option does there.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to spawn the test proxy";
} elseif (!extension_loaded('openssl') || !function_exists('openssl_csr_new')) {
    echo "skip needs openssl to mint the proxy's certificate";
} elseif (!in_array('https', stream_get_wrappers(), true)) {
    echo "skip no https:// wrapper in this build";
}
?>
--FILE--
<?php
require __DIR__ . '/http_wrapper_server.inc';

$dir = http_test_dir('proxy');
$pem = $dir . DIRECTORY_SEPARATOR . 'server.pem';
if (!http_test_make_pem($pem)) {
    echo "no certificate\n";
    http_test_dir_clean($dir);
    return;
}
$port = http_test_proxy_start($proc, $dir, $pem);
if ($port === null) {
    echo "proxy did not start\n";
    http_test_dir_clean($dir);
    return;
}
$proxy = 'tcp://127.0.0.1:' . $port;

function show($label, $url, $http)
{
    global $dir, $proxy;
    http_test_requests_clear($dir);
    $http['proxy'] = $proxy;
    $nConnect = 0;
    $ctx = stream_context_create(array(
        'http' => $http,
        'ssl' => array('verify_peer' => false, 'verify_peer_name' => false),
    ));
    stream_context_set_params($ctx, array('notification' =>
        function ($code) use (&$nConnect) {
            if ($code === STREAM_NOTIFY_CONNECT) {
                $nConnect++;
            }
        }));
    $seen = array();
    set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
    $r = file_get_contents($url, false, $ctx);
    restore_error_handler();
    echo "-- $label\n";
    foreach ($seen as $line) {
        echo '   ! ', $line, "\n";
    }
    echo '   body: ', json_encode($r), ', connects: ', $nConnect, "\n";
    foreach (http_test_requests($dir) as $raw) {
        echo '   > ', json_encode(http_test_scrub($raw)), "\n";
    }
}

show('a tunnelled exchange', 'https://origin.test/p?q=1', array());
show('the origin port the CONNECT names', 'https://origin.test:8443/p', array());
show('Proxy-Authorization moves onto the CONNECT',
    'https://origin.test/p', array('header' => "X-A: 1\r\nProxy-Authorization: Basic Zm9v\r\nX-B: 2"));
show('...from an array entry too',
    'https://origin.test/p', array('header' => array('X-A: 1', 'Proxy-Authorization: Basic QQ==')));
show('...and it can be the only header there',
    'https://origin.test/p', array('header' => 'Proxy-Authorization: Basic QQ=='));
show('a name that only starts like it stays put',
    'https://origin.test/p', array('header' => 'Proxy-Authorization-X: no'));
show('request_fulluri behind the tunnel', 'https://origin.test/p', array('request_fulluri' => true));
show('a proxy that refuses the tunnel', 'https://deny.test/p', array());
show('a plain origin tunnels nothing', 'http://origin.test/p', array('header' => 'X-A: 1'));

http_test_server_stop($proc, $port, true);
http_test_dir_clean($dir);
?>
--EXPECT--
-- a tunnelled exchange
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\n\n"
   > "GET \/p?q=1 HTTP\/1.1\nHost: origin.test\nConnection: close\n\n"
-- the origin port the CONNECT names
   body: "ok", connects: 1
   > "CONNECT origin.test:8443 HTTP\/1.0\n\n"
   > "GET \/p HTTP\/1.1\nHost: origin.test:8443\nConnection: close\n\n"
-- Proxy-Authorization moves onto the CONNECT
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\nProxy-Authorization: Basic Zm9v\n\n"
   > "GET \/p HTTP\/1.1\nHost: origin.test\nConnection: close\nX-A: 1\nX-B: 2\n\n"
-- ...from an array entry too
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\nProxy-Authorization: Basic QQ==\n\n"
   > "GET \/p HTTP\/1.1\nHost: origin.test\nConnection: close\nX-A: 1\n\n"
-- ...and it can be the only header there
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\nProxy-Authorization: Basic QQ==\n\n"
   > "GET \/p HTTP\/1.1\nHost: origin.test\nConnection: close\n\n"
-- a name that only starts like it stays put
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\n\n"
   > "GET \/p HTTP\/1.1\nHost: origin.test\nConnection: close\nProxy-Authorization-X: no\n\n"
-- request_fulluri behind the tunnel
   body: "ok", connects: 1
   > "CONNECT origin.test:443 HTTP\/1.0\n\n"
   > "GET https:\/\/origin.test\/p HTTP\/1.1\nHost: origin.test\nConnection: close\n\n"
-- a proxy that refuses the tunnel
   ! file_get_contents(https://deny.test/p): Failed to open stream: Cannot connect to HTTPS server through proxy
   body: false, connects: 0
   > "CONNECT deny.test:443 HTTP\/1.0\n\n"
-- a plain origin tunnels nothing
   body: "ok", connects: 1
   > "GET \/p HTTP\/1.1\nHost: origin.test\nConnection: close\nX-A: 1\n\n"
