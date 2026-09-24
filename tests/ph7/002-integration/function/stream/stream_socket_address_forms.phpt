--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: how a socket ADDRESS is read — the colon php looks for, the port it accepts, and the flags that bind
--FILE--
<?php
/* php looks for a colon in every position BUT THE LAST, and reads the port with
 * atoi() from the first one it finds. That is the whole difference between an
 * address it refuses and one whose port is simply 0 — the port the OS picks. */
$ssa = stream_socket_server('tcp://127.0.0.1:', $ssa_e, $ssa_es);
var_dump($ssa, $ssa_es);

/* No digits at all is a port of 0, which BINDS. */
$ssa = stream_socket_server('tcp://127.0.0.1:abc', $ssa_e, $ssa_es);
var_dump(is_resource($ssa), (bool)preg_match('/^127\.0\.0\.1:[1-9]\d*$/', stream_socket_get_name($ssa, false)));
fclose($ssa);

/* And the number is taken at SIXTEEN bits, so a port past the range wraps
 * rather than being refused — 65536 is 0, which is "pick one". */
$ssa = stream_socket_server('tcp://127.0.0.1:65536', $ssa_e, $ssa_es);
var_dump(is_resource($ssa), (bool)preg_match('/^127\.0\.0\.1:[1-9]\d*$/', stream_socket_get_name($ssa, false)));
fclose($ssa);

/* An address naming no HOST is a name the resolver is asked about and refuses;
 * it is NOT a wildcard bind, which would put a listener on every interface of
 * the machine. php raises the resolver's own text TWICE — once from the
 * transport and once from the opener that asked. (That text is the OS's
 * gai_strerror(), so only php's part of the sentence around it is asserted.
 * Windows' resolver ACCEPTS the empty name, so php binds there: asserted on
 * POSIX only.) */
if (DIRECTORY_SEPARATOR === '/') {
    $ssa_w = [];
    set_error_handler(function ($n, $m) use (&$ssa_w) { $ssa_w[] = $m; return true; });
    $ssa = stream_socket_server(':8080', $ssa_e, $ssa_es);
    restore_error_handler();
    var_dump($ssa, $ssa_e, str_starts_with($ssa_es, 'php_network_getaddresses: getaddrinfo for  failed: '));
    var_dump($ssa_w === ['stream_socket_server(): ' . $ssa_es,
                         'stream_socket_server(): Unable to connect to :8080 (' . $ssa_es . ')']);
} else {
    echo "bool(false)\nint(0)\nbool(true)\nbool(true)\n";
}

/* $flags: php creates the socket while BINDING it, so asking for neither BIND
 * nor LISTEN answers a socket stream with no socket behind it. It has no name,
 * reads false, writes 0, is at end of file, cannot be put in non-blocking mode
 * and accepts nothing — and it does not even resolve the host. */
$ssa = stream_socket_server('tcp://127.0.0.1:0', $ssa_e, $ssa_es, 0);
var_dump(is_resource($ssa), stream_socket_get_name($ssa, false), $ssa_es);
var_dump(fwrite($ssa, 'x'), fread($ssa, 4), feof($ssa), stream_set_blocking($ssa, false));
$ssa_w = [];
set_error_handler(function ($n, $m) use (&$ssa_w) { $ssa_w[] = $m; return true; });
var_dump(stream_socket_accept($ssa, 0.1));
restore_error_handler();
/* The reason is the OS's strerror() for a timeout, which differs per platform. */
var_dump(count($ssa_w) === 1 && str_starts_with($ssa_w[0], 'stream_socket_accept(): Accept failed: '));
fclose($ssa);
$ssa = stream_socket_server(':1', $ssa_e, $ssa_es, 0);
var_dump(is_resource($ssa), $ssa_es);
fclose($ssa);
?>
--EXPECTF--
%AWarning:%Astream_socket_server(): Unable to connect to tcp://127.0.0.1: (Failed to parse address "127.0.0.1:")%Abool(false)
string(36) "Failed to parse address "127.0.0.1:""
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
int(0)
bool(true)
bool(true)
bool(true)
bool(false)
string(0) ""
int(0)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
string(0) ""
