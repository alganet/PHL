<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The local HTTP server the ext/curl transfer tests talk to.
 *
 * Everything ext/curl answers about a REQUEST -- the status line, the response
 * headers, a redirect chain, a chunked body, the bytes the engine actually put
 * on the wire -- needs something on the other end of a socket. The `-S` server
 * cannot be that something: the tests here have to see the raw request BYTES
 * and to answer replies no dev server would ever produce (a bare 302, a
 * chunked body, a deliberate stall), so this is a raw socket server of its own,
 * written in the engine under test and spawned as a child of the test.
 *
 * It is one file in two modes. Run as a SCRIPT with `--serve <port>` it is the
 * server; INCLUDED from a test it is the three client helpers. The child is
 * therefore whatever binary the runner is testing -- PHPT_TARGET_EXECUTABLE --
 * so the same test file also runs under real php, which is where its
 * expectations come from.
 *
 * No test here reaches the network: the socket binds 127.0.0.1 and the port is
 * probed free first (the server corpus lost half a gate run to a port an editor
 * held). Every wait is bounded -- the accept, the readiness poll and each
 * transfer -- so a server that never comes up is a failed assertion rather than
 * a stuck suite.
 */

/**
 * Pick a free port, spawn the server, and wait until it accepts.
 *
 * Answers the port, or null when this platform cannot spawn one (proc_open is
 * POSIX-only in this engine, which is why every caller SKIPIFs on it).
 */
function curl_test_server_start(&$proc, $base = 19600)
{
    $exe = getenv('PHPT_TARGET_EXECUTABLE');
    if ($exe === false || $exe === '') {
        $exe = PHP_BINARY;
    }
    $port = curl_test_server_free_port($base);
    $spec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $proc = proc_open(array($exe, __FILE__, '--serve', (string) $port), $spec, $pipes);
    if (!is_resource($proc)) {
        return null;
    }
    foreach ($pipes as $p) {
        if (is_resource($p)) {
            fclose($p);
        }
    }
    /* Bounded readiness poll: 100 tries of 50 ms is 5 s, well past the ~20 ms
     * this takes, and short enough that a server which never binds fails the
     * test instead of hanging the suite. */
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

/** Ask the server to stop, then make sure the child is gone either way. */
function curl_test_server_stop($proc, $port)
{
    $c = @fsockopen('127.0.0.1', $port, $errno, $errstr, 0.5);
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
function curl_test_server_free_port($base = 19600, $span = 100)
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

/**
 * The whole request, exactly as it arrived, hex-encoded.
 *
 * `/echo` answers this, so a test asserts on the BYTES the engine wrote --
 * request line, header order, body -- without the response's own encoding
 * getting in the way.  Callers unwrap it with curl_test_request().
 */
function curl_test_request($body)
{
    return $body === false ? false : hex2bin(trim($body));
}

/** The request minus the volatile Host: line, for a pinnable expectation. */
function curl_test_scrub($raw, $boundary = '--BOUNDARY--')
{
    $raw = preg_replace('/^Host: [^\r\n]*\r\n/m', "Host: SERVER\r\n", $raw);
    $raw = preg_replace('/-{10,}[A-Za-z0-9]+/', $boundary, $raw);
    return str_replace("\r\n", "\n", $raw);
}

/* ===== the server half ===== */

function curl_test_server_main($port)
{
    $srv = @stream_socket_server("tcp://127.0.0.1:$port", $errno, $errstr);
    if ($srv === false) {
        fwrite(STDERR, "bind failed: $errstr\n");
        exit(1);
    }
    /* A run that loses its client stops on its own: 20 s per accept, and the
     * whole server is one connection at a time, which is all a single test
     * needs and keeps the reads unambiguous. */
    for ($served = 0; $served < 64; $served++) {
        $c = @stream_socket_accept($srv, 20);
        if ($c === false) {
            break;
        }
        if (curl_test_server_serve($c) === false) {
            fclose($c);
            break;
        }
        fclose($c);
    }
    fclose($srv);
}

/** Read one request, answer it. FALSE means the client asked us to stop. */
function curl_test_server_serve($c)
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
    $chunked = false;
    foreach (explode("\r\n", $head) as $l) {
        if (stripos($l, 'content-length:') === 0) {
            $len = (int) trim(substr($l, 15));
        } elseif (stripos($l, 'transfer-encoding:') === 0 && stripos($l, 'chunked') !== false) {
            $chunked = true;
        } elseif (stripos($l, 'expect:') === 0 && stripos($l, '100-continue') !== false) {
            fwrite($c, "HTTP/1.1 100 Continue\r\n\r\n");
        }
    }
    $body = $chunked ? curl_test_server_read_chunked($c) : curl_test_server_read_len($c, $len);

    $path = '/';
    if (preg_match('#^[A-Z]+ (\S+)#', $head, $m)) {
        $path = $m[1];
    }
    if ($path === '/quit') {
        curl_test_server_reply($c, 200, "bye", array());
        return false;
    }
    if ($path === '/echo') {
        curl_test_server_reply($c, 200, bin2hex($head . $body), array('Content-Type: text/plain'));
        return true;
    }
    if (preg_match('#^/status/(\d+)#', $path, $m)) {
        curl_test_server_reply($c, (int) $m[1], 'status ' . $m[1], array());
        return true;
    }
    if ($path === '/redirect' || $path === '/redirect2') {
        $to = $path === '/redirect2' ? '/redirect' : '/echo';
        curl_test_server_reply($c, 302, '', array('Location: ' . $to));
        return true;
    }
    if ($path === '/headers') {
        curl_test_server_reply($c, 200, 'headed', array(
            'Content-Type: text/plain; charset=iso-8859-1',
            'X-First: one',
            'X-Second: two',
        ));
        return true;
    }
    if ($path === '/auth') {
        $sent = '';
        if (preg_match('/^Authorization: *(\S+) *(\S*)/mi', $head, $m)) {
            $sent = $m[1] . ' ' . ($m[1] === 'Basic' ? base64_decode($m[2]) : '(opaque)');
        }
        if ($sent === '') {
            curl_test_server_reply($c, 401, 'who?', array('WWW-Authenticate: Basic realm="phl"'));
        } else {
            curl_test_server_reply($c, 200, 'hello ' . $sent, array());
        }
        return true;
    }
    if ($path === '/chunked') {
        fwrite($c, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
            . "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n");
        fwrite($c, "5\r\nchunk\r\n3\r\ned!\r\n0\r\n\r\n");
        return true;
    }
    if ($path === '/slow') {
        fwrite($c, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
            . "Content-Length: 4\r\nConnection: close\r\n\r\n");
        sleep(3);
        fwrite($c, "slow");
        return true;
    }
    curl_test_server_reply($c, 404, 'nope', array());
    return true;
}

function curl_test_server_read_len($c, $len)
{
    $body = '';
    while (strlen($body) < $len) {
        $r = fread($c, $len - strlen($body));
        if ($r === '' || $r === false) {
            break;
        }
        $body .= $r;
    }
    return $body;
}

function curl_test_server_read_chunked($c)
{
    $body = '';
    while (true) {
        $line = fgets($c, 64);
        if ($line === false) {
            break;
        }
        $size = hexdec(trim($line));
        if ($size < 1) {
            fgets($c, 64);
            break;
        }
        $body .= curl_test_server_read_len($c, $size);
        fgets($c, 64);
    }
    return $body;
}

function curl_test_server_reply($c, $code, $body, array $headers)
{
    static $reason = array(
        200 => 'OK', 204 => 'No Content', 302 => 'Found',
        404 => 'Not Found', 418 => 'I am a teapot', 500 => 'Internal Server Error',
    );
    $out = 'HTTP/1.1 ' . $code . ' ' . (isset($reason[$code]) ? $reason[$code] : 'Status') . "\r\n";
    foreach ($headers as $h) {
        $out .= $h . "\r\n";
    }
    $out .= 'Content-Length: ' . strlen($body) . "\r\nConnection: close\r\n\r\n" . $body;
    fwrite($c, $out);
}

/*
 * The mode switch, at the BOTTOM so that every function above it is declared
 * before the server loop runs -- a file included for its helpers must not
 * depend on when an engine hoists a top-level declaration.
 */
if (isset($argv[0], $argv[1], $argv[2])
    && $argv[1] === '--serve' && basename($argv[0]) === basename(__FILE__)) {
    curl_test_server_main((int) $argv[2]);
    exit(0);
}
