<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The TLS listener the crypto-transport server tests talk to.
 *
 * A handshake needs two processes: both ends block on the other's first
 * record, so a single script that accepts and then connects deadlocks before
 * either side sends anything. This is the SERVER end, run as a child.
 *
 * It is one file in two modes, like the http:// one beside it: run as a SCRIPT
 * with `--serve <port> <pem>` it listens, INCLUDED from a test it is the
 * helpers. The child is whatever binary the runner is testing
 * (PHPT_TARGET_EXECUTABLE), so the same test runs under real php, which is
 * where its expectations come from.
 *
 * Nothing here reaches the network: the socket binds 127.0.0.1, the port is
 * probed free first, and every wait is bounded.
 */

/** A throwaway self-signed certificate and its key in one PEM, php's own shape
 *  for `local_cert`. Minted per run rather than committed, so nothing here has
 *  an expiry date a later clock can walk past. */
function tls_test_make_pem($path)
{
    $key = openssl_pkey_new(array('private_key_bits' => 2048,
        'private_key_type' => OPENSSL_KEYTYPE_RSA));
    if ($key === false) {
        return false;
    }
    $csr = openssl_csr_new(array('commonName' => 'localhost'), $key,
        array('digest_alg' => 'sha256'));
    if ($csr === false) {
        return false;
    }
    $crt = openssl_csr_sign($csr, null, $key, 2, array('digest_alg' => 'sha256'));
    if ($crt === false) {
        return false;
    }
    $pem = '';
    $pk = '';
    if (!openssl_x509_export($crt, $pem) || !openssl_pkey_export($key, $pk)) {
        return false;
    }
    return file_put_contents($path, $pem . $pk) !== false;
}

/** A port nothing is listening on yet. */
function tls_test_free_port($base)
{
    for ($port = $base; $port < $base + 200; $port++) {
        $probe = @stream_socket_server('tcp://127.0.0.1:' . $port, $errno, $errstr);
        if ($probe !== false) {
            fclose($probe);
            return $port;
        }
    }
    return 0;
}

/** Spawn the listener and wait for it to say it is bound.
 *
 *  The readiness signal is a line on the child's stdout rather than a probe
 *  connection, because a probe would be the connection the child ACCEPTS: it
 *  speaks no TLS, so the one handshake the child has to give would be spent on
 *  a peer that closed. */
function tls_test_server_start(&$proc, $pem, $transport = 'tls', $base = 19760)
{
    $exe = getenv('PHPT_TARGET_EXECUTABLE');
    if ($exe === false || $exe === '') {
        $exe = PHP_BINARY;
    }
    $port = tls_test_free_port($base);
    if ($port === 0) {
        return null;
    }
    $spec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $proc = proc_open(array($exe, __FILE__, '--serve', (string) $port, $pem, $transport),
        $spec, $pipes);
    if (!is_resource($proc)) {
        return null;
    }
    fclose($pipes[0]);
    fclose($pipes[2]);
    stream_set_blocking($pipes[1], false);
    $seen = '';
    for ($i = 0; $i < 200; $i++) {
        $seen .= (string) fread($pipes[1], 64);
        if (strpos($seen, "READY\n") !== false) {
            fclose($pipes[1]);
            return $port;
        }
        if (feof($pipes[1])) {
            break;
        }
        usleep(25000);
    }
    fclose($pipes[1]);
    return null;
}

/** Reap the child, whatever state it reached. */
function tls_test_server_stop($proc)
{
    if (is_resource($proc)) {
        proc_terminate($proc);
        proc_close($proc);
    }
}

if (isset($argv[1]) && $argv[1] === '--serve') {
    $port = (int) $argv[2];
    $pem = $argv[3];
    $transport = isset($argv[4]) ? $argv[4] : 'tls';
    $ctx = stream_context_create(array('ssl' => array('local_cert' => $pem)));
    $srv = @stream_socket_server($transport . '://127.0.0.1:' . $port, $errno, $errstr,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $ctx);
    if ($srv === false) {
        exit(1);
    }
    /* The parent is blocked on this line; nothing else is ever written here. */
    echo "READY\n";
    flush();
    /* One connection, echoed back with a prefix so the client can tell the
     * bytes came through the negotiated session and not from a proxy. */
    $conn = @stream_socket_accept($srv, 5, $peer);
    if ($conn === false) {
        fclose($srv);
        exit(2);
    }
    $in = fread($conn, 32);
    $reply = 'echo:' . $in
        . '|peer=' . (preg_match('/^127\.0\.0\.1:\d+$/', (string) $peer) ? 'ok' : 'bad')
        . '|ctx=' . implode(',', array_keys(stream_context_get_options($conn)))
        . '|type=' . stream_get_meta_data($conn)['stream_type'];
    fwrite($conn, $reply);
    fclose($conn);
    fclose($srv);
    exit(0);
}
