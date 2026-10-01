--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the LISTENING half of ssl:// and tls:// — binding, the per-accept handshake, and the context an accept inherits
--SKIPIF--
<?php
if (!function_exists('proc_open')) {
    echo "skip needs proc_open to spawn the listener";
} elseif (!function_exists('openssl_csr_sign')) {
    echo "skip needs openssl to mint a throwaway certificate";
} elseif (stripos(PHP_OS, 'WIN') === 0) {
    echo "skip posix-only test harness";
}
?>
--FILE--
<?php
require __DIR__ . '/tls_server.inc';

/* A crypto transport's LISTENER is an ordinary bound TCP socket: php does not
 * negotiate anything here, so the address binds and the handshake belongs to
 * each connection accepted through it. That is why the four spellings differ
 * only in which protocols a later handshake may settle on. */
foreach (array('ssl', 'tls', 'tlsv1.2', 'tlsv1.3') as $sst_x) {
    $sst_s = @stream_socket_server("$sst_x://127.0.0.1:0", $sst_e, $sst_es,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN);
    printf("%-8s %s %s %s\n", $sst_x, is_resource($sst_s) ? 'resource' : 'false',
        var_export($sst_es, true),
        is_resource($sst_s) ? stream_get_meta_data($sst_s)['stream_type'] : '-');
    if (is_resource($sst_s)) {
        fclose($sst_s);
    }
}

/* The listener reports the address it was OPENED with, transport and all, and
 * names itself by the bound host:port the way a tcp:// one does. */
$sst_srv = stream_socket_server('tls://127.0.0.1:0', $sst_e2, $sst_es2,
    STREAM_SERVER_BIND | STREAM_SERVER_LISTEN);
$sst_m = stream_get_meta_data($sst_srv);
var_dump((bool) preg_match('#^tls://127\.0\.0\.1:\d+$#', $sst_m['uri']));
var_dump((bool) preg_match('/^127\.0\.0\.1:\d+$/', stream_socket_get_name($sst_srv, false)));
fclose($sst_srv);

/* A `local_cert` that names nothing still BINDS: the file is read by the
 * handshake, and there has not been one yet. */
$sst_bad = @stream_socket_server('tls://127.0.0.1:0', $sst_e3, $sst_es3,
    STREAM_SERVER_BIND | STREAM_SERVER_LISTEN,
    stream_context_create(array('ssl' => array('local_cert' => '/nonexistent.pem'))));
var_dump(is_resource($sst_bad), $sst_e3, $sst_es3);
fclose($sst_bad);

/* An ACCEPTED connection inherits the listener's context — for a plain tcp://
 * server as much as for a crypto one, which is what makes the `ssl` options a
 * tls:// listener was created with reach the handshake at all. */
$sst_ctx = stream_context_create(array('http' => array('method' => 'POST'),
    'ssl' => array('peer_name' => 'localhost')));
$sst_p = stream_socket_server('tcp://127.0.0.1:0', $sst_e4, $sst_es4,
    STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $sst_ctx);
$sst_c = stream_socket_client('tcp://' . stream_socket_get_name($sst_p, false));
$sst_a = stream_socket_accept($sst_p, 2);
var_dump(stream_context_get_options($sst_a) === stream_context_get_options($sst_p));
var_dump(json_encode(stream_context_get_options($sst_a)));
fclose($sst_a);
fclose($sst_c);
fclose($sst_p);

/* And the round trip. The child listens on tls:// with a throwaway
 * self-signed certificate; this end dials the same transport and the bytes
 * only arrive if both halves negotiated. */
$sst_pem = tempnam(sys_get_temp_dir(), 'phltls') . '.pem';
var_dump(tls_test_make_pem($sst_pem));
$sst_proc = null;
$sst_port = tls_test_server_start($sst_proc, $sst_pem);
var_dump($sst_port !== null);
$sst_cli = @stream_socket_client("tls://127.0.0.1:$sst_port", $sst_e5, $sst_es5, 5,
    STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('verify_peer' => false,
        'verify_peer_name' => false))));
var_dump(is_resource($sst_cli), $sst_e5, $sst_es5);
fwrite($sst_cli, 'ping');
/* What the SERVER end saw: the bytes, that its peer name is the client's
 * address, that its context is the listener's, and the ops it reports. */
echo stream_get_contents($sst_cli), "\n";
var_dump(stream_get_meta_data($sst_cli)['stream_type']);
fclose($sst_cli);
tls_test_server_stop($sst_proc);
@unlink($sst_pem);

/* A listener with NO certificate has nothing to offer the handshake, so the
 * ACCEPT is what fails — the bind never could. php reports it twice and
 * answers false, leaving the peer-name out-param exactly as it was. */
$sst_proc2 = null;
$sst_port2 = tls_test_server_start($sst_proc2, '/nonexistent.pem', 'tls', 19960);
var_dump($sst_port2 !== null);
$sst_cli2 = @stream_socket_client("tls://127.0.0.1:$sst_port2", $sst_e6, $sst_es6, 5,
    STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('verify_peer' => false,
        'verify_peer_name' => false))));
var_dump($sst_cli2);
tls_test_server_stop($sst_proc2);
?>
--EXPECT--
ssl      resource '' tcp_socket/ssl
tls      resource '' tcp_socket/ssl
tlsv1.2  resource '' tcp_socket/ssl
tlsv1.3  resource '' tcp_socket/ssl
bool(true)
bool(true)
bool(true)
int(0)
string(0) ""
bool(true)
string(58) "{"http":{"method":"POST"},"ssl":{"peer_name":"localhost"}}"
bool(true)
bool(true)
bool(true)
int(0)
string(0) ""
echo:ping|peer=ok|ctx=ssl|type=tcp_socket/ssl
string(14) "tcp_socket/ssl"
bool(true)
bool(false)
