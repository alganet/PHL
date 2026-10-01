--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the peer certificate a context captures, and the fingerprint a script pins
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

/* Warnings are collected rather than printed, so their COUNT and ORDER are
 * part of the expectation. The port a listener happened to get is normalised
 * away: it is the only thing in these sentences that moves between runs. */
$scc_warn = array();
set_error_handler(function ($scc_no, $scc_msg) use (&$scc_warn) {
    $scc_warn[] = preg_replace('/:\d+ /', ':PORT ', $scc_msg);
    return true;
});
function scc_warnings()
{
    global $scc_warn;
    foreach ($scc_warn as $scc_w) {
        echo "  W: $scc_w\n";
    }
    $scc_warn = array();
}

$scc_pem = tempnam(sys_get_temp_dir(), 'phlcc') . '.pem';
var_dump(tls_test_make_pem($scc_pem));
$scc_raw = file_get_contents($scc_pem);
$scc_sha1 = openssl_x509_fingerprint($scc_raw, 'sha1');
$scc_md5 = openssl_x509_fingerprint($scc_raw, 'md5');
$scc_sha256 = openssl_x509_fingerprint($scc_raw, 'sha256');

/* One dial per listener: the helper serves a single connection, which is what
 * makes each case below independent of the one before it. The context is
 * handed back because the capture is written onto IT and not onto the handle. */
$scc_base = 20440;
function scc_dial($ssl, &$ctx = null)
{
    global $scc_pem, $scc_base;
    $scc_base += 4;
    $proc = null;
    $port = tls_test_server_start($proc, $scc_pem, 'tls', $scc_base);
    if ($port === null) {
        echo "no listener\n";
        return false;
    }
    $ssl += array('verify_peer' => false, 'verify_peer_name' => false);
    $ctx = stream_context_create(array('ssl' => $ssl));
    $cli = @stream_socket_client("tls://127.0.0.1:$port", $e, $es, 5,
        STREAM_CLIENT_CONNECT, $ctx);
    $ok = is_resource($cli);
    if ($ok) {
        fclose($cli);
    }
    tls_test_server_stop($proc);
    /* A CLOSED handle is not a resource any more, so what the caller is told
     * is whether the dial came back with one -- not the handle itself. */
    return $ok;
}
/* Only the `ssl` sub-array is ever printed, and only its KEY ORDER: the
 * captured names are APPENDED to whatever the script set, so a context that
 * asked for nothing carries nothing extra. */
function scc_keys($ctx)
{
    $o = stream_context_get_options($ctx);
    echo '  ssl: ', implode(',', array_keys($o['ssl'])), "\n";
    return $o['ssl'];
}

echo "-- capture_peer_cert\n";
$scc_ctx = null;
var_dump(scc_dial(array('capture_peer_cert' => true), $scc_ctx));
$scc_ssl = scc_keys($scc_ctx);
/* php hands over the extension's own handle class, so every ext/openssl door
 * takes it -- which is the whole point of capturing it. */
echo '  ', get_debug_type($scc_ssl['peer_certificate']), "\n";
var_dump(openssl_x509_parse($scc_ssl['peer_certificate'])['subject']['CN']);
var_dump(openssl_x509_fingerprint($scc_ssl['peer_certificate'], 'sha1') === $scc_sha1);

echo "-- capture_peer_cert_chain\n";
var_dump(scc_dial(array('capture_peer_cert_chain' => true), $scc_ctx));
$scc_ssl = scc_keys($scc_ctx);
$scc_chain = $scc_ssl['peer_certificate_chain'];
var_dump(is_array($scc_chain), count($scc_chain));
echo '  ', get_debug_type($scc_chain[0]), "\n";
var_dump(openssl_x509_fingerprint($scc_chain[0], 'sha1') === $scc_sha1);

echo "-- both options present but FALSE\n";
var_dump(scc_dial(array('capture_peer_cert' => false,
    'capture_peer_cert_chain' => false), $scc_ctx));
scc_keys($scc_ctx);

/* The STRING form names its algorithm by LENGTH -- 32 characters is md5, 40 is
 * sha1 -- and the comparison is case-insensitive at both ends. A sha256 hex
 * string is 64 characters, which names nothing at all: php answers a plain
 * mismatch for it rather than an unknown-digest complaint, so the string form
 * simply cannot express sha256. */
echo "-- peer_fingerprint, the string form\n";
foreach (array('sha1' => $scc_sha1, 'sha1 uppercase' => strtoupper($scc_sha1),
         'md5' => $scc_md5, 'sha256 (64 characters)' => $scc_sha256,
         'wrong' => str_repeat('a', 40), 'empty' => '') as $scc_what => $scc_fp) {
    echo "  $scc_what: ";
    var_dump(scc_dial(array('peer_fingerprint' => $scc_fp)));
    scc_warnings();
}

/* The ARRAY form names the algorithm itself, so sha256 is reachable there, and
 * EVERY entry has to match: one wrong digest beside a right one refuses. */
echo "-- peer_fingerprint, the array form\n";
foreach (array(
    'sha256' => array('sha256' => $scc_sha256),
    'algorithm name uppercase' => array('SHA1' => $scc_sha1),
    'two, both right' => array('sha256' => $scc_sha256, 'sha1' => $scc_sha1),
    'two, one wrong' => array('sha256' => $scc_sha256, 'md5' => str_repeat('0', 32)),
    'wrong one first' => array('md5' => str_repeat('0', 32), 'sha1' => $scc_sha1),
    'unknown algorithm' => array('nosuch' => 'x'),
    'right one, then an unknown algorithm' => array('sha1' => $scc_sha1, 'nosuch' => 'x'),
    'no entries at all' => array(),
    'an integer key' => array(0 => $scc_sha1),
    'a value that is not a string' => array('sha256' => 12345),
) as $scc_what => $scc_fp) {
    echo "  $scc_what: ";
    var_dump(scc_dial(array('peer_fingerprint' => $scc_fp)));
    scc_warnings();
}

/* Neither a string nor an array is refused before the session is asked for
 * anything, and that sentence is the ONLY one: no match failure follows it. */
echo "-- peer_fingerprint, neither a string nor an array\n";
foreach (array('an integer' => 12345, 'null' => null, 'false' => false) as $scc_what => $scc_fp) {
    echo "  $scc_what: ";
    var_dump(scc_dial(array('peer_fingerprint' => $scc_fp)));
    scc_warnings();
}

/* The capture happens BEFORE the pin is checked, so a refused peer still
 * leaves the certificate that was refused on the context -- which is how a
 * script finds out what it was actually offered. */
echo "-- a refused fingerprint still captures\n";
var_dump(scc_dial(array('capture_peer_cert' => true,
    'peer_fingerprint' => str_repeat('a', 40)), $scc_ctx));
scc_warnings();
$scc_ssl = scc_keys($scc_ctx);
var_dump(openssl_x509_fingerprint($scc_ssl['peer_certificate'], 'sha1') === $scc_sha1);

/* A LISTENER that pins a fingerprint is asking about a certificate its client
 * was never asked to send, and php names that shortage rather than calling it
 * a mismatch. */
echo "-- a listener pinning a client it never asked for a certificate\n";
$scc_srv = @stream_socket_server('tls://127.0.0.1:0', $scc_e, $scc_es,
    STREAM_SERVER_BIND | STREAM_SERVER_LISTEN,
    stream_context_create(array('ssl' => array('local_cert' => $scc_pem,
        'peer_fingerprint' => str_repeat('a', 40)))));
$scc_name = stream_socket_get_name($scc_srv, false);
$scc_exe = getenv('PHPT_TARGET_EXECUTABLE');
if ($scc_exe === false || $scc_exe === '') {
    $scc_exe = PHP_BINARY;
}
$scc_kid = tempnam(sys_get_temp_dir(), 'phlccc') . '.php';
file_put_contents($scc_kid, '<?php @stream_socket_client("tls://' . $scc_name
    . '", $e, $s, 5, STREAM_CLIENT_CONNECT, stream_context_create(array("ssl"'
    . ' => array("verify_peer" => false, "verify_peer_name" => false))));'
    . ' usleep(300000);');
$scc_kidp = proc_open(array($scc_exe, $scc_kid),
    array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w')),
    $scc_pipes);
fclose($scc_pipes[0]);
var_dump(stream_socket_accept($scc_srv, 5));
scc_warnings();
fclose($scc_srv);
proc_terminate($scc_kidp);
proc_close($scc_kidp);
@unlink($scc_kid);
@unlink($scc_pem);
?>
--EXPECT--
bool(true)
-- capture_peer_cert
bool(true)
  ssl: capture_peer_cert,verify_peer,verify_peer_name,peer_certificate
  OpenSSLCertificate
string(9) "localhost"
bool(true)
-- capture_peer_cert_chain
bool(true)
  ssl: capture_peer_cert_chain,verify_peer,verify_peer_name,peer_certificate_chain
bool(true)
int(1)
  OpenSSLCertificate
bool(true)
-- both options present but FALSE
bool(true)
  ssl: capture_peer_cert,capture_peer_cert_chain,verify_peer,verify_peer_name
-- peer_fingerprint, the string form
  sha1: bool(true)
  sha1 uppercase: bool(true)
  md5: bool(true)
  sha256 (64 characters): bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  wrong: bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  empty: bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
-- peer_fingerprint, the array form
  sha256: bool(true)
  algorithm name uppercase: bool(true)
  two, both right: bool(true)
  two, one wrong: bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  wrong one first: bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  unknown algorithm: bool(false)
  W: stream_socket_client(): Unknown digest algorithm
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  right one, then an unknown algorithm: bool(false)
  W: stream_socket_client(): Unknown digest algorithm
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  no entries at all: bool(false)
  W: stream_socket_client(): Invalid peer_fingerprint array; [algo => fingerprint] form required
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  an integer key: bool(false)
  W: stream_socket_client(): Invalid peer_fingerprint array; [algo => fingerprint] form required
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  a value that is not a string: bool(false)
  W: stream_socket_client(): Invalid peer_fingerprint array; [algo => fingerprint] form required
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
-- peer_fingerprint, neither a string nor an array
  an integer: bool(false)
  W: stream_socket_client(): Expected peer fingerprint must be a string or an array
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  null: bool(false)
  W: stream_socket_client(): Expected peer fingerprint must be a string or an array
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  false: bool(false)
  W: stream_socket_client(): Expected peer fingerprint must be a string or an array
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
-- a refused fingerprint still captures
bool(false)
  W: stream_socket_client(): peer_fingerprint match failure
  W: stream_socket_client(): Failed to enable crypto
  W: stream_socket_client(): Unable to connect to tls://127.0.0.1:PORT (Unknown error)
  ssl: capture_peer_cert,peer_fingerprint,verify_peer,verify_peer_name,peer_certificate
bool(true)
-- a listener pinning a client it never asked for a certificate
bool(false)
  W: stream_socket_accept(): Could not get peer certificate
  W: stream_socket_accept(): Failed to enable crypto
  W: stream_socket_accept(): Accept failed: Cannot enable crypto
