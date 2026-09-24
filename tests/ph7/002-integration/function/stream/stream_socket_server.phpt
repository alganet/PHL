--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_socket_server / stream_socket_accept / stream_socket_get_name — a php program serving a port
--FILE--
<?php
/* The three names a php program becomes a SERVER through. Every one of them
 * was a `Call to undefined function`, so listening on a port could not be
 * spelled at all — even though the engine had bind() and listen() all along. */
$srv = stream_socket_server('tcp://127.0.0.1:0', $sss_errno, $sss_errstr);
var_dump(is_resource($srv), $sss_errno, $sss_errstr);

/* Port 0 asks the OS to pick one, and get_name() is the only way to learn
 * which — the whole reason a test can bind without guessing a free port. */
$sss_local = stream_socket_get_name($srv, false);
var_dump((bool)preg_match('/^127\.0\.0\.1:\d+$/', $sss_local));
/* Nothing has connected, so the server has no peer. */
var_dump(stream_socket_get_name($srv, true));

$sss_port = (int)substr($sss_local, strrpos($sss_local, ':') + 1);
$cli = stream_socket_client('tcp://127.0.0.1:' . $sss_port, $sss_e2, $sss_es2, 5);
var_dump(is_resource($cli));
var_dump(stream_socket_get_name($cli, true) === $sss_local);

$conn = stream_socket_accept($srv, 5, $sss_peer);
var_dump(is_resource($conn));
/* The accepted end's peer IS the client's own local address. */
var_dump($sss_peer === stream_socket_get_name($cli, false));

fwrite($cli, 'ping');
var_dump(fread($conn, 4));
fwrite($conn, 'pong');
var_dump(fread($cli, 4));

/* What the two ends SAY about themselves: the server was opened by name and
 * reports it, and an accepted connection was opened by nobody — so php reports
 * neither a uri nor a wrapper for it. */
$sss_m = stream_get_meta_data($conn);
var_dump($sss_m['stream_type'], array_key_exists('uri', $sss_m), array_key_exists('wrapper_type', $sss_m));
var_dump(stream_get_meta_data($srv)['uri']);

/* A wait that EXPIRES is a warning plus false, not an exception and not a
 * hang: this is what lets a single-threaded server do something else between
 * connections. The out-param is left alone. */
$sss_w = [];
set_error_handler(function ($n, $m) use (&$sss_w) { $sss_w[] = [$n, $m]; return true; });
$sss_t = microtime(true);
$sss_none = stream_socket_accept($srv, 0.2, $sss_p2);
restore_error_handler();
var_dump($sss_none, microtime(true) - $sss_t >= 0.1, $sss_p2);
/* The reason after the colon is the OS's strerror() for a timeout. */
var_dump(count($sss_w) === 1 && $sss_w[0][0] === E_WARNING
    && str_starts_with($sss_w[0][1], 'stream_socket_accept(): Accept failed: '));

fclose($conn);
fclose($cli);
fclose($srv);

/* BIND without LISTEN: php keeps the two flags apart, so this is a bound
 * socket nothing can connect to. */
$sss_bind = stream_socket_server('tcp://127.0.0.1:0', $sss_e3, $sss_es3, STREAM_SERVER_BIND);
var_dump(is_resource($sss_bind), (bool)preg_match('/^127\.0\.0\.1:\d+$/', stream_socket_get_name($sss_bind, false)));
fclose($sss_bind);
var_dump(STREAM_SERVER_BIND, STREAM_SERVER_LISTEN);
?>
--EXPECT--
bool(true)
int(0)
string(0) ""
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
string(4) "ping"
string(4) "pong"
string(14) "tcp_socket/ssl"
bool(false)
bool(false)
string(17) "tcp://127.0.0.1:0"
bool(false)
bool(true)
NULL
bool(true)
bool(true)
bool(true)
int(4)
int(8)
