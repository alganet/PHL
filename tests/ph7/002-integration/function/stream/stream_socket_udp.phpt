--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: udp:// binds, connects, and carries a datagram both ways
--FILE--
<?php
/* The DATAGRAM transport. Until this shipped, `udp://` was php's own
 * "Unable to find the socket transport" at every door, so the recvfrom/sendto
 * pair had no unconnected socket to use and a program that speaks a datagram
 * protocol could not be written at all. */
$say = function ($n, $s) { echo "  ERR[$n] ", rtrim($s), "\n"; return true; };
set_error_handler($say);
/* Two answers below are the SOCKET STACK's rather than php's, and the two
 * stacks disagree; each is asserted against the one its own OS gives. */
$win = DIRECTORY_SEPARATOR === '\\';

/* php's two server flags are separate BECAUSE of this transport: its ops
 * refuse STREAM_XPORT_OP_LISTEN with a flat -1 and log nothing, so the DEFAULT
 * $flags fail with no reason of their own -- which the warning fills in with
 * the words `Unknown error` while $errstr stays the empty string. */
$bad = stream_socket_server('udp://127.0.0.1:0', $be, $bes);
var_dump($bad, $be, $bes);

$srv = stream_socket_server('udp://127.0.0.1:0', $errno, $errstr, STREAM_SERVER_BIND);
var_dump(is_resource($srv), $errno, $errstr);
$name = stream_socket_get_name($srv, false);
var_dump((bool)preg_match('/^127\.0\.0\.1:\d+$/', $name));
/* A bound datagram socket is connected to nobody, so it has no peer. */
var_dump(stream_socket_get_name($srv, true));

$meta = stream_get_meta_data($srv);
var_dump($meta['stream_type'], $meta['mode'], $meta['seekable'], $meta['eof']);

$cli = stream_socket_client("udp://$name", $ce, $ces);
var_dump(is_resource($cli), $ce, $ces);
var_dump(stream_socket_get_name($cli, true) === $name);
var_dump(stream_get_meta_data($cli)['stream_type']);

/* Two writes are two DATAGRAMS, and each read takes one whole datagram or the
 * front of it -- the rest of a truncated one is dropped, not queued. */
var_dump(fwrite($cli, 'hello'), fwrite($cli, 'world'));
var_dump(stream_socket_recvfrom($srv, 100, 0, $peer));
var_dump($peer === stream_socket_get_name($cli, false));
var_dump(stream_socket_recvfrom($srv, 3, 0, $peer));
stream_set_blocking($srv, false);
var_dump(stream_socket_recvfrom($srv, 100, 0, $peer), $peer);

/* And back the other way, to the address the datagram carried. */
stream_set_blocking($srv, true);
fwrite($cli, 'ping');
stream_socket_recvfrom($srv, 100, 0, $peer);
var_dump(stream_socket_sendto($srv, 'pong', 0, $peer));
var_dump(fread($cli, 100));

/* An empty datagram is a datagram -- but only from the door that sends one:
 * fwrite() of an empty string answers 0 without reaching the socket at all,
 * where stream_socket_sendto() puts a zero-length datagram on the wire. */
var_dump(fwrite($cli, ''));
var_dump(stream_socket_sendto($cli, ''));
var_dump(stream_socket_recvfrom($srv, 100, 0, $peer), $peer === stream_socket_get_name($cli, false));

/* STREAM_PEEK looks without taking. */
fwrite($cli, 'peekme');
var_dump(stream_socket_recvfrom($srv, 100, STREAM_PEEK, $peer));
var_dump(stream_socket_recvfrom($srv, 100, 0, $peer));

/* There is nothing to accept on a socket that never listened, and nothing to
 * shut down on one that was never connected. */
/* The timeout's own wording is the stack's, so only the severity is kept. */
set_error_handler(function ($n) { echo "  ERR[$n]\n"; return true; });
var_dump(stream_socket_accept($srv, 0));
set_error_handler($say);
/* Shutting down a socket that was never CONNECTED is ENOTCONN on POSIX and
 * accepted on Windows. */
var_dump(stream_socket_shutdown($srv, STREAM_SHUT_RDWR) === $win);

/* A write with no destination at all is the OS's own refusal, which php
 * reports as a NOTICE and answers false for. */
$un = stream_socket_server('udp://127.0.0.1:0', $errno, $errstr, STREAM_SERVER_BIND);
/* WHICH refusal it is stays the stack's own word for it (EDESTADDRREQ here,
 * WSAENOTCONN there), so only the SEVERITY of each is printed -- a notice for
 * the write and a warning for the sendto, which is php's own split. */
set_error_handler(function ($n) { echo "  ERR[$n]\n"; return true; });
var_dump(fwrite($un, 'nowhere'));
var_dump(stream_socket_sendto($un, 'nowhere'));
set_error_handler($say);
fclose($un); fclose($cli); fclose($srv);
?>
--EXPECT--
  ERR[2] stream_socket_server(): Unable to connect to udp://127.0.0.1:0 (Unknown error)
bool(false)
int(0)
string(0) ""
bool(true)
int(0)
string(0) ""
bool(true)
bool(false)
string(10) "udp_socket"
string(2) "r+"
bool(false)
bool(false)
bool(true)
int(0)
string(0) ""
bool(true)
string(10) "udp_socket"
int(5)
int(5)
string(5) "hello"
bool(true)
string(3) "wor"
bool(false)
NULL
int(4)
string(4) "pong"
int(0)
int(0)
string(0) ""
bool(true)
string(6) "peekme"
string(6) "peekme"
  ERR[2]
bool(false)
bool(true)
  ERR[8]
bool(false)
  ERR[2]
int(-1)
--CLEAN--
<?php
unset($srv, $cli, $un, $bad, $name, $meta, $peer, $errno, $errstr, $ce, $ces, $be, $bes, $win, $say);
