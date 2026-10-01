--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what a negotiated crypto handle says about itself, what a second enable answers, and the three warnings a refused handshake leaves
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

/* Warnings are collected rather than printed, so the COUNT and the ORDER are
 * part of the expectation and neither depends on where the ini puts them. */
$scs_warn = array();
set_error_handler(function ($scs_no, $scs_msg) use (&$scs_warn) {
    $scs_warn[] = $scs_msg;
    return true;
});
function scs_warnings()
{
    global $scs_warn;
    foreach ($scs_warn as $scs_w) {
        echo "  W: $scs_w\n";
    }
    $scs_warn = array();
}

$scs_pem = tempnam(sys_get_temp_dir(), 'phlcs') . '.pem';
var_dump(tls_test_make_pem($scs_pem));
$scs_proc = null;
$scs_port = tls_test_server_start($scs_proc, $scs_pem, 'tls', 20240);
var_dump($scs_port !== null);
$scs_cli = stream_socket_client("tls://127.0.0.1:$scs_port", $scs_e, $scs_es, 5,
    STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('verify_peer' => false,
        'verify_peer_name' => false))));
var_dump(is_resource($scs_cli));

/* A LIVE session is the only thing that puts php's `crypto` sub-array on a
 * handle, and php builds it before the three keys every other device starts
 * with -- so it LEADS the key order rather than joining the end. */
$scs_m = stream_get_meta_data($scs_cli);
echo implode(',', array_keys($scs_m)), "\n";
echo implode(',', array_keys($scs_m['crypto'])), "\n";
/* The protocol is php's spelling of the version, not OpenSSL's; the cipher's
 * three answers are its own, and only their SHAPE is stable across builds. */
var_dump((bool) preg_match('/^(TLSv1|TLSv1\.[123]|SSLv3|UNKNOWN)$/', $scs_m['crypto']['protocol']));
var_dump(is_string($scs_m['crypto']['cipher_name']), $scs_m['crypto']['cipher_name'] !== '');
var_dump(is_int($scs_m['crypto']['cipher_bits']), $scs_m['crypto']['cipher_bits'] > 0);
var_dump($scs_m['crypto']['protocol'] === $scs_m['crypto']['cipher_version']);

/* A handle that has already carried a session refuses every later enable, and
 * the refusal is FALSE both times: the WARNING is the blocking handle's alone,
 * because on a non-blocking one the same state means "the handshake this loop
 * is driving is already up". */
var_dump(stream_socket_enable_crypto($scs_cli, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
scs_warnings();
stream_set_blocking($scs_cli, false);
var_dump(stream_socket_enable_crypto($scs_cli, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
scs_warnings();
stream_set_blocking($scs_cli, true);

/* Disabling really does tear the session down -- the sub-array is gone
 * afterwards -- and still answers FALSE, because the value that reaches the
 * script is the one that means "no handshake completed in this call". */
var_dump(stream_socket_enable_crypto($scs_cli, false));
echo implode(',', array_keys(stream_get_meta_data($scs_cli))), "\n";
/* And the handle stays refused: php frees nothing until it closes, so the
 * session it no longer uses is still what the next enable trips over. */
var_dump(stream_socket_enable_crypto($scs_cli, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
scs_warnings();
var_dump(stream_socket_enable_crypto($scs_cli, false));
scs_warnings();
fclose($scs_cli);
tls_test_server_stop($scs_proc);

/* A `local_cert` that names nothing is a CONTEXT failure, not a handshake one:
 * php names the file in a sentence of its own and never reads OpenSSL's queue
 * for it. Three doors reach the same failure and each adds its own sentence --
 * the accept says it could not accept, the connect that it could not connect,
 * and stream_socket_enable_crypto() says nothing at all beyond the layer's own
 * text. */
echo "-- accept\n";
$scs_srv = stream_socket_server('tls://127.0.0.1:0', $scs_e2, $scs_es2,
    STREAM_SERVER_BIND | STREAM_SERVER_LISTEN,
    stream_context_create(array('ssl' => array('local_cert' => '/nonexistent.pem'))));
$scs_peer = stream_socket_client('tcp://' . stream_socket_get_name($scs_srv, false));
var_dump(stream_socket_accept($scs_srv, 2));
scs_warnings();
fclose($scs_peer);
fclose($scs_srv);

echo "-- connect\n";
$scs_plain = stream_socket_server('tcp://127.0.0.1:0');
$scs_bad = stream_socket_client('ssl://' . stream_socket_get_name($scs_plain, false),
    $scs_e3, $scs_es3, 2, STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('local_cert' => '/nonexistent.pem',
        'verify_peer' => false))));
var_dump($scs_bad, $scs_e3, $scs_es3);
scs_warnings();
fclose($scs_plain);

echo "-- enable_crypto\n";
$scs_plain2 = stream_socket_server('tcp://127.0.0.1:0');
$scs_up = stream_socket_client('tcp://' . stream_socket_get_name($scs_plain2, false),
    $scs_e4, $scs_es4, 2, STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('local_cert' => '/nonexistent.pem',
        'verify_peer' => false))));
var_dump(stream_socket_enable_crypto($scs_up, true, STREAM_CRYPTO_METHOD_TLS_CLIENT));
scs_warnings();
fclose($scs_up);
fclose($scs_plain2);

/* A failed HANDSHAKE is the other shape: php's sentence, then ONE
 * `OpenSSL Error messages:` heading with everything OpenSSL queued under it --
 * one heading however many errors it drained. The peer here is an ordinary TCP
 * listener that answers the ClientHello with plaintext. */
echo "-- handshake\n";
$scs_proc2 = null;
$scs_port2 = tls_test_server_start($scs_proc2, $scs_pem, 'tcp', 20340);
var_dump($scs_port2 !== null);
$scs_no = stream_socket_client("tls://127.0.0.1:$scs_port2", $scs_e5, $scs_es5, 5,
    STREAM_CLIENT_CONNECT,
    stream_context_create(array('ssl' => array('verify_peer' => false,
        'verify_peer_name' => false))));
var_dump($scs_no);
foreach ($scs_warn as $scs_w) {
    echo "  W: ", str_replace("\n", '\n', $scs_w), "\n";
}
$scs_warn = array();
tls_test_server_stop($scs_proc2);
@unlink($scs_pem);
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
crypto,timed_out,blocked,eof,stream_type,mode,unread_bytes,seekable,uri
protocol,cipher_name,cipher_bits,cipher_version
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
  W: stream_socket_enable_crypto(): SSL/TLS already set-up for this stream
bool(false)
bool(false)
timed_out,blocked,eof,stream_type,mode,unread_bytes,seekable,uri
bool(false)
  W: stream_socket_enable_crypto(): SSL/TLS already set-up for this stream
bool(false)
-- accept
bool(false)
  W: stream_socket_accept(): Unable to set local cert chain file `/nonexistent.pem'; Check that your cafile/capath settings include details of your certificate and its issuer
  W: stream_socket_accept(): Failed to enable crypto
  W: stream_socket_accept(): Accept failed: Cannot enable crypto
-- connect
bool(false)
int(0)
string(0) ""
  W: stream_socket_client(): Unable to set local cert chain file `/nonexistent.pem'; Check that your cafile/capath settings include details of your certificate and its issuer
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to ssl://127.0.0.1:%d (Unknown error)
-- enable_crypto
bool(false)
  W: stream_socket_enable_crypto(): Unable to set local cert chain file `/nonexistent.pem'; Check that your cafile/capath settings include details of your certificate and its issuer
-- handshake
bool(true)
bool(false)
  W: stream_socket_client(): SSL operation failed with code 1. OpenSSL Error messages:\nerror:%A
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:%d (Unknown error)
