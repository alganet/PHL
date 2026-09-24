--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what stream_socket_server / accept / get_name refuse, and how each refusal is worded
--FILE--
<?php
/* The two halves of an address have a diagnostic each, and neither is the
 * other's: a transport this build does not carry is not a malformed address. */
$ssr_srv = stream_socket_server('tcp://127.0.0.1', $ssr_e, $ssr_es);
var_dump($ssr_srv, $ssr_e, $ssr_es);

$ssr_srv = stream_socket_server('bogus://host:1', $ssr_e, $ssr_es);
var_dump($ssr_srv, $ssr_e, $ssr_es);

/* An address is a string, and php CASTS what it is given rather than answering
 * false in silence — a port on its own names no host and no port. */
$ssr_srv = stream_socket_server(8080, $ssr_e, $ssr_es);
var_dump($ssr_srv, $ssr_es);

/* A stream that is not a socket cannot be accepted on or named. */
$ssr_fh = fopen(__FILE__, 'r');
var_dump(stream_socket_accept($ssr_fh, 0.1));
var_dump(stream_socket_get_name($ssr_fh, false), stream_socket_get_name($ssr_fh, true));
fclose($ssr_fh);

/* And a handle that is not a stream at all is a TypeError naming the
 * parameter, both for a wrong type and for a closed one. */
try { stream_socket_accept(7, 0.1); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { stream_socket_get_name(7, false); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$ssr_closed = fopen(__FILE__, 'r');
fclose($ssr_closed);
try { stream_socket_get_name($ssr_closed, false); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECTF--
%AWarning:%Astream_socket_server(): Unable to connect to tcp://127.0.0.1 (Failed to parse address "127.0.0.1")%Abool(false)
int(0)
string(35) "Failed to parse address "127.0.0.1""
%AWarning:%Astream_socket_server(): Unable to connect to bogus://host:1 (Unable to find the socket transport "bogus" - did you forget to enable it when you configured PHP?)%Abool(false)
int(0)
string(98) "Unable to find the socket transport "bogus" - did you forget to enable it when you configured PHP?"
%AWarning:%Astream_socket_server(): Unable to connect to 8080 (Failed to parse address "8080")%Abool(false)
string(30) "Failed to parse address "8080""
%AWarning:%Astream_socket_accept(): Accept failed: Unknown error%Abool(false)
bool(false)
bool(false)
stream_socket_accept(): Argument #1 ($socket) must be of type resource, int given
stream_socket_get_name(): Argument #1 ($socket) must be of type resource, int given
stream_socket_get_name(): Argument #1 ($socket) must be an open stream resource
--CLEAN--
<?php
unset($ssr_srv, $ssr_e, $ssr_es, $ssr_fh, $ssr_closed, $e);
