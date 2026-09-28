--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the bracketed IPv6 form binds, connects and names itself back
--SKIPIF--
<?php
if (@stream_socket_server('tcp://[::1]:0') === false) {
    echo "skip no IPv6 loopback on this host";
}
?>
--FILE--
<?php
/* php parses the BRACKETED form before it looks for a port at all, and the name
 * it answers keeps the brackets — which is the same spelling every address
 * argument in the family reads back. This engine used to build a sockaddr_in by
 * hand for every bind, so `[::1]:0` reached the resolver as the one-byte host
 * `[` and was refused; and the two name reporters read a sockaddr_in out of a
 * socket that had none, so a connection made over IPv6 through an ordinary
 * HOSTNAME (which has always been possible here) answered false for both of its
 * own addresses. */
$srv = stream_socket_server('tcp://[::1]:0', $errno, $errstr);
var_dump(is_resource($srv), $errno, $errstr);
$name = stream_socket_get_name($srv, false);
var_dump((bool)preg_match('/^\[::1\]:\d+$/', $name));

$cli = stream_socket_client("tcp://$name", $ce, $ces);
var_dump(is_resource($cli), $ce, $ces);
var_dump((bool)preg_match('/^\[::1\]:\d+$/', stream_socket_get_name($cli, false)));
var_dump(stream_socket_get_name($cli, true) === $name);

$acc = stream_socket_accept($srv, 1, $peer);
var_dump($peer === stream_socket_get_name($cli, false));
var_dump(fwrite($cli, 'six'), fread($acc, 8));
fclose($acc); fclose($cli); fclose($srv);

/* The datagram half, whose peer name comes back off each datagram. */
$us = stream_socket_server('udp://[::1]:0', $errno, $errstr, STREAM_SERVER_BIND);
$un = stream_socket_get_name($us, false);
var_dump((bool)preg_match('/^\[::1\]:\d+$/', $un));
$uc = stream_socket_client("udp://$un", $ce, $ces);
var_dump(fwrite($uc, 'dgram'));
var_dump(stream_socket_recvfrom($us, 20, 0, $from));
var_dump($from === stream_socket_get_name($uc, false));
var_dump(stream_socket_sendto($us, 'back', 0, $from), fread($uc, 20));
fclose($uc); fclose($us);

/* And the shapes php refuses at the parse, which have a wording of their own. */
foreach (['tcp://[::1]', 'tcp://[::1:9', 'tcp://[]', 'tcp://[::1]x:9'] as $bad) {
    $r = @stream_socket_client($bad, $be, $bes);
    var_dump($r, $bes);
}
?>
--EXPECTF--
bool(true)
int(0)
string(0) ""
bool(true)
bool(true)
int(0)
string(0) ""
bool(true)
bool(true)
bool(true)
int(3)
string(3) "six"
bool(true)
int(5)
string(5) "dgram"
bool(true)
int(4)
string(4) "back"
bool(false)
string(36) "Failed to parse IPv6 address "[::1]""
bool(false)
string(37) "Failed to parse IPv6 address "[::1:9""
bool(false)
string(33) "Failed to parse IPv6 address "[]""
bool(false)
string(39) "Failed to parse IPv6 address "[::1]x:9""
--CLEAN--
<?php
unset($srv, $cli, $acc, $us, $uc, $name, $un, $peer, $from, $errno, $errstr, $ce, $ces, $be, $bes, $r, $bad);
