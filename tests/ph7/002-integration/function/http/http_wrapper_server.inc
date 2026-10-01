<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The local HTTP server the http:// STREAM WRAPPER tests talk to.
 *
 * The wrapper's contract is about bytes on both sides of the socket -- which
 * request it composed, and what it makes of a response nobody would call
 * well-formed -- so a dev server is no use here. This is a raw socket server
 * that answers each request with a CANNED reply the test wrote to a file, and
 * logs the raw request bytes so the test can assert on them.
 *
 * It is one file in two modes, like the ext/curl one beside it: run as a SCRIPT
 * with `--serve <port> <dir>` it is the server, INCLUDED from a test it is the
 * client helpers. The child is whatever binary the runner is testing
 * (PHPT_TARGET_EXECUTABLE), so the same test file runs under real php, which is
 * where its expectations come from.
 *
 * Nothing here reaches the network: the socket binds 127.0.0.1 and the port is
 * probed free first. Every wait is bounded.
 */

/** Pick a free port, spawn the server on it, and wait until it accepts.
 *
 *  With $pem the listener is a TLS one, which is how the https:// wrapper is
 *  tested: same canned replies, same request log, a handshake in front. */
function http_test_server_start(&$proc, $dir, $base = 19700, $pem = null)
{
    $exe = getenv('PHPT_TARGET_EXECUTABLE');
    if ($exe === false || $exe === '') {
        $exe = PHP_BINARY;
    }
    $port = http_test_free_port($base);
    $spec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $argv = array($exe, __FILE__, '--serve', (string) $port, $dir);
    if ($pem !== null) {
        $argv[] = $pem;
    }
    $proc = proc_open($argv, $spec, $pipes);
    if (!is_resource($proc)) {
        return null;
    }
    if ($pem !== null) {
        /* A TLS listener cannot be probed by connecting: the probe would BE
         * the connection it accepts, and a peer that speaks no TLS spends the
         * handshake the test needs. The child says when it is bound instead. */
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
    foreach ($pipes as $p) {
        if (is_resource($p)) {
            fclose($p);
        }
    }
    for ($i = 0; $i < 100; $i++) {
        $probe = @fsockopen('127.0.0.1', $port, $errno, $errstr, 0.2);
        if ($probe !== false) {
            fclose($probe);
            return $port;
        }
        usleep(50000);
    }
    return null;
}

/** Spawn the CONNECT PROXY on a free port and wait until it accepts.
 *
 *  Same canned replies and same request log as the plain listener; what is
 *  different is the door. A request that arrives as `CONNECT` is answered with
 *  a tunnel -- 200, then TLS on the very socket the CONNECT came in on, so the
 *  proxy stands in for the origin without a second process -- and anything else
 *  is served in the clear, which is what a proxied http:// request looks like. */
function http_test_proxy_start(&$proc, $dir, $pem, $base = 19860)
{
    $exe = getenv('PHPT_TARGET_EXECUTABLE');
    if ($exe === false || $exe === '') {
        $exe = PHP_BINARY;
    }
    $port = http_test_free_port($base);
    $spec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $proc = proc_open(array($exe, __FILE__, '--proxy', (string) $port, $dir, $pem),
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

/** A throwaway self-signed certificate and its key in one PEM, php's own shape
 *  for `local_cert`. Minted per run rather than committed, so nothing here has
 *  an expiry date a later clock can walk past. */
function http_test_make_pem($path)
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

/** Ask the server to stop, then make sure the child is gone either way. */
function http_test_server_stop($proc, $port, $tls = false)
{
    /* A TLS listener has no plain door to ask through: it is terminated. */
    $c = $tls ? false : @fsockopen('127.0.0.1', $port, $errno, $errstr, 0.5);
    if ($c !== false) {
        fwrite($c, "GET /quit HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
        stream_set_timeout($c, 1);
        while (!feof($c)) {
            if (fread($c, 4096) === '') {
                break;
            }
        }
        fclose($c);
    }
    if (is_resource($proc)) {
        proc_terminate($proc);
        proc_close($proc);
    }
}

/** A port on 127.0.0.1 nothing is listening on right now. */
function http_test_free_port($base = 19700, $span = 100)
{
    $port = $base + (getmypid() % $span);
    for ($i = 0; $i < $span; $i++) {
        $probe = @fsockopen('127.0.0.1', $port, $errno, $errstr, 0.2);
        if ($probe === false) {
            return $port;
        }
        fclose($probe);
        $port = $base + (($port - $base + 1) % $span);
    }
    return $port;
}

/** A scratch directory for this test's canned replies. */
function http_test_dir($tag)
{
    $dir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phlhttp' . $tag . getmypid();
    if (!is_dir($dir)) {
        mkdir($dir);
    }
    return $dir;
}

/** Remove it again, replies and request log included. */
function http_test_dir_clean($dir)
{
    foreach (glob($dir . DIRECTORY_SEPARATOR . '*') as $f) {
        @unlink($f);
    }
    @rmdir($dir);
}

/** Arm `/canned/<name>` with these exact response bytes. */
function http_test_reply($dir, $name, $bytes)
{
    file_put_contents($dir . DIRECTORY_SEPARATOR . $name, $bytes);
}

/** The raw requests the server has been handed since the last clear. */
function http_test_requests($dir)
{
    $log = $dir . DIRECTORY_SEPARATOR . 'log';
    if (!is_file($log)) {
        return array();
    }
    $out = array();
    foreach (file($log, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
        $out[] = hex2bin(trim($line));
    }
    return $out;
}

function http_test_requests_clear($dir)
{
    @unlink($dir . DIRECTORY_SEPARATOR . 'log');
}

/** The volatile parts out: this run's port, and CRLF made visible. */
function http_test_scrub($raw)
{
    $raw = preg_replace('/127\.0\.0\.1:\d+/', 'HOST', $raw);
    return str_replace("\r\n", "\n", $raw);
}

/* ===== the server half ===== */

/** The proxy loop. Every request it is handed is logged, tunnelled or not. */
function http_test_proxy_main($port, $dir, $pem)
{
    $ctx = stream_context_create(array('ssl' => array('local_cert' => $pem)));
    $srv = @stream_socket_server("tcp://127.0.0.1:$port", $errno, $errstr,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $ctx);
    if ($srv === false) {
        fwrite(STDERR, "bind failed: $errstr\n");
        exit(1);
    }
    /* The parent is blocked on this line; nothing else is written here. */
    echo "READY\n";
    flush();
    for ($served = 0; $served < 64; $served++) {
        $c = @stream_socket_accept($srv, 20);
        if ($c === false) {
            break;
        }
        stream_set_timeout($c, 20);
        $head = '';
        while (($line = fgets($c, 8192)) !== false) {
            $head .= $line;
            if ($line === "\r\n" || $line === "\n") {
                break;
            }
        }
        if (strncasecmp($head, 'CONNECT ', 8) !== 0) {
            /* Not a tunnel: this is a proxied plain request, and the proxy is
             * the only server it will ever reach. */
            if ($head === '') {
                fclose($c);
                continue;
            }
            http_test_proxy_serve($c, $dir, $head);
            fclose($c);
            continue;
        }
        file_put_contents($dir . DIRECTORY_SEPARATOR . 'log',
            bin2hex($head) . "\n", FILE_APPEND);
        /* The tunnel a proxy REFUSES is asked for by name: a CONNECT is the
         * one request that carries nothing of the script's own to key on. */
        if (strncasecmp($head, 'CONNECT deny.', 13) === 0) {
            fwrite($c, "HTTP/1.0 407 Proxy Authentication Required\r\n"
                . "Proxy-Authenticate: Basic realm=\"p\"\r\n\r\n");
            fclose($c);
            continue;
        }
        fwrite($c, "HTTP/1.0 200 Connection established\r\n\r\n");
        if (@stream_socket_enable_crypto($c, true, STREAM_CRYPTO_METHOD_TLS_SERVER) !== true) {
            fclose($c);
            continue;
        }
        $req = '';
        while (($line = fgets($c, 8192)) !== false) {
            $req .= $line;
            if ($line === "\r\n" || $line === "\n") {
                break;
            }
        }
        http_test_proxy_serve($c, $dir, $req);
        fclose($c);
    }
    fclose($srv);
}

/** Log one request and answer it exactly as the plain listener would. */
function http_test_proxy_serve($c, $dir, $head)
{
    file_put_contents($dir . DIRECTORY_SEPARATOR . 'log', bin2hex($head) . "\n", FILE_APPEND);
    $path = '/';
    if (preg_match('#^\S+ (\S+)#', $head, $m)) {
        $path = $m[1];
    }
    if (preg_match('#/canned/([A-Za-z0-9_.-]+)#', $path, $m)
        && is_file($dir . DIRECTORY_SEPARATOR . $m[1])) {
        fwrite($c, file_get_contents($dir . DIRECTORY_SEPARATOR . $m[1]));
        return;
    }
    fwrite($c, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n"
        . "Connection: close\r\n\r\nok");
}

function http_test_server_main($port, $dir, $pem = null)
{
    if ($pem !== null) {
        $ctx = stream_context_create(array('ssl' => array('local_cert' => $pem)));
        $srv = @stream_socket_server("tls://127.0.0.1:$port", $errno, $errstr,
            STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $ctx);
    } else {
        $srv = @stream_socket_server("tcp://127.0.0.1:$port", $errno, $errstr);
    }
    if ($srv === false) {
        fwrite(STDERR, "bind failed: $errstr\n");
        exit(1);
    }
    if ($pem !== null) {
        /* The parent is blocked on this line; nothing else is written here. */
        echo "READY\n";
        flush();
    }
    $n = 0;
    for ($served = 0; $served < 256; $served++) {
        $c = @stream_socket_accept($srv, 20);
        if ($c === false) {
            break;
        }
        if (http_test_server_serve($c, $dir) === false) {
            fclose($c);
            break;
        }
        fclose($c);
    }
    fclose($srv);
}

/** Read one request, answer it. FALSE means the client asked us to stop. */
function http_test_server_serve($c, $dir)
{
    stream_set_timeout($c, 20);
    $head = '';
    while (($line = fgets($c, 8192)) !== false) {
        $head .= $line;
        if ($line === "\r\n" || $line === "\n") {
            break;
        }
    }
    if ($head === '') {
        return true;
    }
    $len = 0;
    foreach (explode("\r\n", $head) as $l) {
        if (stripos($l, 'content-length:') === 0) {
            $len = (int) trim(substr($l, 15));
        }
    }
    $body = '';
    while (strlen($body) < $len) {
        $r = fread($c, $len - strlen($body));
        if ($r === '' || $r === false) {
            break;
        }
        $body .= $r;
    }
    $path = '/';
    if (preg_match('#^\S+ (\S+)#', $head, $m)) {
        $path = $m[1];
    }
    if ($path === '/quit') {
        fwrite($c, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nbye");
        return false;
    }
    file_put_contents($dir . DIRECTORY_SEPARATOR . 'log', bin2hex($head . $body) . "\n", FILE_APPEND);
    if (preg_match('#/canned/([A-Za-z0-9_.-]+)#', $path, $m)
        && is_file($dir . DIRECTORY_SEPARATOR . $m[1])) {
        fwrite($c, file_get_contents($dir . DIRECTORY_SEPARATOR . $m[1]));
        return true;
    }
    /* Everything else answers the request BYTES, so a test can assert on what
     * the wrapper actually wrote without the response getting in the way. */
    $hex = bin2hex($head . $body);
    fwrite($c, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
        . strlen($hex) . "\r\nConnection: close\r\n\r\n" . $hex);
    return true;
}

/*
 * The mode switch, at the BOTTOM so every function above it is declared before
 * the server loop runs.
 */
if (isset($argv[0], $argv[1], $argv[2], $argv[3])
    && basename($argv[0]) === basename(__FILE__)) {
    if ($argv[1] === '--serve') {
        http_test_server_main((int) $argv[2], $argv[3], isset($argv[4]) ? $argv[4] : null);
        exit(0);
    }
    if ($argv[1] === '--proxy' && isset($argv[4])) {
        http_test_proxy_main((int) $argv[2], $argv[3], $argv[4]);
        exit(0);
    }
}
