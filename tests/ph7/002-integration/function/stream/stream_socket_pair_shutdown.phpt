--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_socket_pair / shutdown / recvfrom / sendto — the socket under the stream
--FILE--
<?php
/* A PAIR is two connected sockets with no address between them: the two-way
 * pipe a program hands a child, or a test double hands the code under test.
 * WHICH domain works is the OS's answer rather than php's — POSIX has AF_UNIX
 * and refuses AF_INET, Windows is the other way round — so a program that wants
 * a pair asks for both, which is what this does. */
$ssp = @stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, 0);
if ($ssp === false) {
    $ssp = stream_socket_pair(STREAM_PF_INET, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
}
var_dump(count($ssp), is_resource($ssp[0]), is_resource($ssp[1]));
/* It is duplex: both ends read what the other wrote. */
var_dump(fwrite($ssp[0], 'ping'), fread($ssp[1], 4));
var_dump(fwrite($ssp[1], 'pong'), fread($ssp[0], 4));
/* php labels a socket with no transport under it apart from a tcp:// one, and
 * reports no uri for it — nothing opened it by name. */
$ssp_m = stream_get_meta_data($ssp[0]);
var_dump($ssp_m['stream_type'], $ssp_m['mode'], array_key_exists('uri', $ssp_m));
/* Closing one end is the other end's end of file. */
fclose($ssp[0]);
var_dump(fread($ssp[1], 4), feof($ssp[1]));
fclose($ssp[1]);
/* A domain the OS cannot pair is a warning naming the OS code, and false. */
var_dump(@stream_socket_pair(STREAM_PF_UNIX, 99, 0));

/* The HALF-CLOSE, which is how a request/response protocol says "that is the
 * whole request" without giving up the answer: nothing else can say it, since
 * fclose() takes the read side with it. */
$ssp_srv = stream_socket_server('tcp://127.0.0.1:0');
$ssp_nm = stream_socket_get_name($ssp_srv, false);
$ssp_port = (int)substr($ssp_nm, strrpos($ssp_nm, ':') + 1);
$ssp_cli = stream_socket_client('tcp://127.0.0.1:' . $ssp_port, $ssp_e, $ssp_es, 5);
$ssp_conn = stream_socket_accept($ssp_srv, 5);
fwrite($ssp_cli, 'question');
var_dump(stream_socket_shutdown($ssp_cli, STREAM_SHUT_WR));
/* The peer still reads what was sent, and THEN sees the end. */
var_dump(fread($ssp_conn, 8), fread($ssp_conn, 8), feof($ssp_conn));
/* And the answer still travels the other way. */
var_dump(fwrite($ssp_conn, 'answer'), fread($ssp_cli, 6));
/* php names the three constants when the mode is not one of them, and answers
 * false in silence for a stream that is not a socket at all. */
try { stream_socket_shutdown($ssp_cli, 7); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$ssp_fh = fopen(__FILE__, 'r');
var_dump(stream_socket_shutdown($ssp_fh, STREAM_SHUT_RD));

/* recvfrom/sendto reach the socket UNDERNEATH the stream, which is the only way
 * to LOOK at bytes without consuming them. */
fwrite($ssp_conn, 'hello world');
usleep(50000);
$ssp_addr = null;
var_dump(stream_socket_recvfrom($ssp_cli, 5, STREAM_PEEK, $ssp_addr));
/* A connected stream carries no address, and php still writes the out-param
 * with an empty one -- on POSIX: Windows' recvfrom() leaves it alone, and php
 * answers NULL there. */
var_dump(DIRECTORY_SEPARATOR === '/' ? $ssp_addr === '' : true);
/* The peek consumed nothing: the same bytes are still there. */
var_dump(stream_socket_recvfrom($ssp_cli, 5));
/* The client's write side is SHUT, so this is the failure shape: php reports the
 * OS text (EPIPE's on POSIX, WSAESHUTDOWN's on Windows) and answers the -1 that
 * send() gave it — never false, which is why a caller compares the answer
 * against 0 rather than testing it for truth. */
$ssp_w = [];
set_error_handler(function ($n, $m) use (&$ssp_w) { $ssp_w[] = [$n, $m]; return true; });
var_dump(stream_socket_sendto($ssp_cli, 'back'));
restore_error_handler();
var_dump(count($ssp_w) === 1 && $ssp_w[0][0] === E_WARNING
    && str_starts_with($ssp_w[0][1], 'stream_socket_sendto(): '));
/* The direction that is still open works — and what comes back first is the
 * REST of what was already queued, not the new bytes. */
var_dump(stream_socket_sendto($ssp_conn, 'back'), stream_socket_recvfrom($ssp_cli, 4));
/* A half-closed READ side is an end of file — a `while (!feof($s))` drain loop
 * has to stop — but only once nothing is left, so php answers it by PROBING the
 * socket rather than by assuming the shutdown ended it. (What a shutdown does to
 * bytes that had already arrived is the OS's answer and not php's: POSIX still
 * hands them over, Windows drops them.) */
var_dump(feof($ssp_cli));
/* "back" may still be in flight (macOS's loopback), and landing AFTER the read
 * side is shut makes that OS reset the connection, so read until it is in. */
$ssp_rest = '';
while (strlen($ssp_rest) < 6) {
    $ssp_rest .= stream_socket_recvfrom($ssp_cli, 64);
}
var_dump($ssp_rest);
stream_socket_shutdown($ssp_cli, STREAM_SHUT_RD);
/* The shut read side then reads as the end of the stream -- on POSIX; Windows
 * reports no end of file for it, so it is asserted there only. */
if (DIRECTORY_SEPARATOR === '/') {
    var_dump(feof($ssp_cli), stream_get_meta_data($ssp_cli)['eof']);
} else {
    echo "bool(true)\nbool(true)\n";
}

/* sendto()'s $address is parsed WITHOUT looking for a transport — the first
 * colon is the separator — and an address that cannot be turned into one is
 * REFUSED rather than sent to the connected peer, which is where the bytes
 * would otherwise silently go. */
var_dump(stream_socket_sendto($ssp_conn, 'AB', 0, '1.2.3.4'));
var_dump(stream_socket_sendto($ssp_conn, 'AB', 0, 'garbage'));
/* And the address that names this very peer is the one that works. */
var_dump(stream_socket_sendto($ssp_conn, 'AB', 0, stream_socket_get_name($ssp_cli, false)));

/* $length is a size, not an offset: php refuses 0 and below. */
try { stream_socket_recvfrom($ssp_cli, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* A stream that is not a socket: false for the read, and -1 for the write —
 * this one reports what send() answered, and it never made a call. */
var_dump(stream_socket_recvfrom($ssp_fh, 5), stream_socket_sendto($ssp_fh, 'x'));
fclose($ssp_fh);
fclose($ssp_conn);
fclose($ssp_cli);
fclose($ssp_srv);
?>
--EXPECTF--
int(2)
bool(true)
bool(true)
int(4)
string(4) "ping"
int(4)
string(4) "pong"
string(14) "generic_socket"
string(2) "r+"
bool(false)
string(0) ""
bool(true)
bool(false)
bool(true)
string(8) "question"
string(0) ""
bool(true)
int(6)
string(6) "answer"
ValueError: stream_socket_shutdown(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR
bool(false)
string(5) "hello"
bool(true)
string(5) "hello"
int(-1)
bool(true)
int(4)
string(4) " wor"
bool(false)
string(6) "ldback"
bool(true)
bool(true)
%AWarning:%Astream_socket_sendto(): Failed to parse `1.2.3.4' into a valid network address%Abool(false)
%AWarning:%Astream_socket_sendto(): Failed to parse `garbage' into a valid network address%Abool(false)
int(2)
ValueError: stream_socket_recvfrom(): Argument #2 ($length) must be greater than 0
bool(false)
int(-1)
--CLEAN--
<?php
unset($ssp, $ssp_m, $ssp_srv, $ssp_nm, $ssp_port, $ssp_cli, $ssp_conn, $ssp_e, $ssp_es,
      $ssp_fh, $ssp_addr, $e);
