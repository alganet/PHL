--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_socket_client()'s $flags, pfsockopen()'s persistence, and the timeout every socket carries
--FILE--
<?php
/* $flags was declared in the signature and read by NOTHING, and its three
 * constants were undefined — so the documented spellings were fatals and the
 * literal was accepted and dropped. */
var_dump(STREAM_CLIENT_CONNECT, STREAM_CLIENT_ASYNC_CONNECT, STREAM_CLIENT_PERSISTENT);

$scf_srv = stream_socket_server('tcp://127.0.0.1:0');
$scf_nm = stream_socket_get_name($scf_srv, false);
$scf_port = (int)substr($scf_nm, strrpos($scf_nm, ':') + 1);

/* php creates the socket while CONNECTING it, so a $flags without
 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no name at
 * either end, reads false, writes 0, and already at end of file. */
$scf_none = stream_socket_client('tcp://127.0.0.1:' . $scf_port, $scf_e, $scf_es, 1, 0);
var_dump(is_resource($scf_none), $scf_es);
var_dump(stream_socket_get_name($scf_none, false), stream_socket_get_name($scf_none, true));
var_dump(fwrite($scf_none, 'x'), fread($scf_none, 4), feof($scf_none));
fclose($scf_none);

/* PERSISTENT is the whole difference between fsockopen() and pfsockopen(), and
 * it is not cosmetic: a second open of the SAME address hands back the very
 * same resource rather than dialling again. */
$scf_a = pfsockopen('127.0.0.1', $scf_port);
$scf_ca = stream_socket_accept($scf_srv, 2);
var_dump(get_resource_type($scf_a));
$scf_b = pfsockopen('127.0.0.1', $scf_port);
var_dump($scf_a === $scf_b, stream_socket_get_name($scf_a, false) === stream_socket_get_name($scf_b, false));
/* The KEY is the address as it was spelled, so another spelling of the same
 * host is another connection. */
$scf_c = pfsockopen('localhost', $scf_port);
$scf_cc = stream_socket_accept($scf_srv, 2);
var_dump($scf_a === $scf_c);
/* A NON-persistent open of the same address is its own connection too, and says
 * so through its type. */
$scf_d = fsockopen('127.0.0.1', $scf_port);
$scf_cd = stream_socket_accept($scf_srv, 2);
var_dump($scf_a === $scf_d, get_resource_type($scf_d));
/* And fclose() ends a persistent stream: the next open dials again. */
var_dump(fclose($scf_a), is_resource($scf_a));
$scf_e2 = pfsockopen('127.0.0.1', $scf_port);
$scf_ce = stream_socket_accept($scf_srv, 2);
var_dump(is_resource($scf_e2), get_resource_type($scf_e2));
/* stream_socket_client() asks for the same thing with the flag. */
$scf_f = stream_socket_client('tcp://127.0.0.1:' . $scf_port, $scf_e, $scf_es, 1,
    STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
$scf_cf = stream_socket_accept($scf_srv, 2);
$scf_g = stream_socket_client('tcp://127.0.0.1:' . $scf_port, $scf_e, $scf_es, 1,
    STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
var_dump($scf_f === $scf_g, get_resource_type($scf_f));

/* Every socket php opens is bounded by `default_socket_timeout` from the start:
 * a read from a peer that has gone quiet comes back FALSE with `timed_out` set,
 * where this engine armed nothing and waited forever. */
ini_set('default_socket_timeout', 1);
$scf_h = stream_socket_client('tcp://127.0.0.1:' . $scf_port, $scf_e, $scf_es, 1);
$scf_ch = stream_socket_accept($scf_srv, 2);
$scf_t = microtime(true);
var_dump(fread($scf_h, 10));
var_dump(microtime(true) - $scf_t >= 0.9, stream_get_meta_data($scf_h)['timed_out'], feof($scf_h));
/* `timed_out` describes the LAST read, so a read that WORKS clears it — one
 * quiet period does not mark the handle for the rest of its life. And it is not
 * fread()'s alone: a line read reports the same expiry. */
fwrite($scf_ch, "line\n");
usleep(50000);
var_dump(fgets($scf_h), stream_get_meta_data($scf_h)['timed_out']);
var_dump(fgets($scf_h), stream_get_meta_data($scf_h)['timed_out']);

/* And the KEY a persistent handle is kept under names the FUNCTION that asked,
 * so a pfsockopen() and a persistent stream_socket_client() of one address are
 * two connections. */
$scf_x = stream_socket_client('127.0.0.1:' . $scf_port, $scf_e, $scf_es, 1,
    STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
$scf_cx = stream_socket_accept($scf_srv, 2);
var_dump($scf_x === pfsockopen('127.0.0.1', $scf_port));

/* A NON-BLOCKING handle is the other case, and it answers "" straight away —
 * an armed timeout does not turn that into a failure. */
stream_set_blocking($scf_h, false);
$scf_t = microtime(true);
var_dump(fread($scf_h, 10), microtime(true) - $scf_t < 0.5);
fclose($scf_h);
fclose($scf_srv);
?>
--EXPECT--
int(4)
int(2)
int(1)
bool(true)
string(0) ""
bool(false)
bool(false)
int(0)
bool(false)
bool(true)
string(17) "persistent stream"
bool(true)
bool(true)
bool(false)
bool(false)
string(6) "stream"
bool(true)
bool(false)
bool(true)
string(17) "persistent stream"
bool(true)
string(17) "persistent stream"
bool(false)
bool(true)
bool(true)
bool(false)
string(5) "line
"
bool(false)
bool(false)
bool(true)
bool(false)
string(0) ""
bool(true)
--CLEAN--
<?php
unset($scf_srv, $scf_nm, $scf_port, $scf_none, $scf_e, $scf_es, $scf_a, $scf_ca, $scf_b,
      $scf_c, $scf_cc, $scf_d, $scf_cd, $scf_e2, $scf_ce, $scf_f, $scf_cf, $scf_g, $scf_h,
      $scf_ch, $scf_t, $scf_x, $scf_cx);
