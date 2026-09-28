--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what a socket address MEANS, and when it is read at all
--FILE--
<?php
/* Five rules the datagram derivation uncovered, every one of them a wrong
 * answer on an ordinary tcp:// program too. */
$say = function ($n, $s) { echo "  ERR[$n] ", rtrim($s), "\n"; return true; };
$mute = function () { return true; };
set_error_handler($say);
/* Four of the answers below belong to the SOCKET STACK rather than to php, and
 * the two stacks disagree; each is asserted against the one its own OS gives,
 * so the printed line is the same either way. */
$win = DIRECTORY_SEPARATOR === '\\';
$linux = PHP_OS_FAMILY === 'Linux';

/* (1) The TRANSPORT is looked up in a hash keyed by the name as WRITTEN, so it
 * is case-SENSITIVE -- unlike a wrapper SCHEME, which php folds first. */
foreach (['tcp', 'TCP', 'Tcp', 'udp', 'UDP'] as $t) {
    set_error_handler($mute);
    $c = @stream_socket_client("$t://127.0.0.1:9", $e, $es);
    set_error_handler($say);
    /* A transport the build has not got is php's OWN sentence, which is the
     * answer under test; one it has got gets as far as the stack, whose
     * refusal each OS words for itself. */
    echo "$t: ", strpos($es, 'socket transport') !== false ? $es
        : ($es === '' ? '<no refusal: a datagram socket connects to anything>'
                      : '<the stack refused the port>'), "\n";
    if (is_resource($c)) { fclose($c); }
}

/* (2) The address is parsed by the BIND and by the CONNECT, so a $flags that
 * asks for neither never looks at it. Only the transport is resolved. */
$a = stream_socket_server('0.0.0.0', $e, $es, STREAM_SERVER_LISTEN);
var_dump(is_resource($a), $e, $es);
$b = stream_socket_client('0.0.0.0', $e, $es, null, 0);
var_dump(is_resource($b), $e, $es);
$c = @stream_socket_server('nope://1.2.3.4:5', $e, $es, 0);
var_dump($c, $es);
fclose($a); fclose($b);

/* (3) An address whose host half is EMPTY is a NAME like any other, and the
 * RESOLVER is what answers for it. This used to be a message of PHL's own,
 * `Empty host` with an errno of -1, that no php prints. What the resolver
 * MAKES of the empty name is its own: POSIX refuses it outright (and php says
 * so twice, once as the resolver's own failure and once as the open's) where
 * Winsock reads it as the loopback, so there the PORT is what refuses. */
set_error_handler($mute);
$d = @stream_socket_client('tcp://:9', $e, $es);
set_error_handler($say);
var_dump($d, $es !== '', $es !== 'Empty host', $e !== -1);

/* (4) A connect that failed reports the OS code it actually got, not a
 * hardcoded ECONNREFUSED: a broadcast address refused for want of
 * SO_BROADCAST and a refused port used to be one answer. Whether a broadcast
 * address is refused AT ALL is the stack's own rule -- Linux wants the
 * permission and answers EACCES without it, macOS and Winsock simply connect
 * -- so what is pinned is that the reason is never the one a refused PORT
 * gives, and the diagnostic is kept out of the output for the same reason. */
set_error_handler($mute);
$f = @stream_socket_client('udp://255.255.255.255:9', $e, $es);
set_error_handler($say);
var_dump(is_resource($f) === !$linux, $es !== 'Connection refused');
if (is_resource($f)) { fclose($f); }

/* (5) STREAM_CLIENT_ASYNC_CONNECT is a SEPARATE bit from STREAM_CLIENT_CONNECT
 * and php's transport dials for EITHER, so a $flags naming only the async one
 * opens a real socket -- it used to answer a socket-LESS handle here, which
 * wrote 0 bytes and named no peer. The dial is issued non-blocking and the
 * handle comes back BLOCKING, so a port nothing is listening on is a resource
 * whose refusal surfaces at the first write. */
$g = @stream_socket_client('tcp://127.0.0.1:9', $e, $es, null, STREAM_CLIENT_ASYNC_CONNECT);
var_dump(is_resource($g), $e, $es);
/* Whether the refusal has ARRIVED by the time the peer is asked for is the
 * stack's timing: POSIX has it already and names nobody, Winsock still has the
 * dial in flight and names the address it dialled. */
var_dump(stream_get_meta_data($g)['blocked'],
    stream_socket_get_name($g, true) === ($win ? '127.0.0.1:9' : false));
set_error_handler($mute);
var_dump(@fwrite($g, 'x'));
set_error_handler($say);
fclose($g);

/* (6) feof() on a socket is a LIVENESS PROBE run when the question is asked,
 * not a latch on a read that already happened -- and stream_get_meta_data()
 * copies the stored flag without running it. */
$srv = stream_socket_server('tcp://127.0.0.1:0', $e, $es);
$nm  = stream_socket_get_name($srv, false);
/* The async dial that does finish: a real listener, in php's blocking mode.
 * Whether it has finished by the time the peer is asked for is the stack's
 * timing again -- Linux's loopback completes it inside the call, macOS may
 * not yet -- so the peer is either the listener or nobody. */
$as = stream_socket_client("tcp://$nm", $e, $es, null, STREAM_CLIENT_ASYNC_CONNECT);
var_dump(is_resource($as), $e, $es);
var_dump(stream_get_meta_data($as)['blocked'], in_array(stream_socket_get_name($as, true), [$nm, false], true));
$aa = stream_socket_accept($srv, 1);
var_dump(fwrite($as, 'async'), fread($aa, 8));
fclose($aa); fclose($as);

$cli = stream_socket_client("tcp://$nm", $e, $es);
$acc = stream_socket_accept($srv, 1);
var_dump(feof($acc));
fclose($cli);
/* The far end's FIN arriving is the stack's timing -- Linux's loopback has it
 * at once, macOS's not always -- so wait until it has (a select reads nothing,
 * and leaves the stored flag alone) before the probe is asked. */
$fin = array($acc); $none = null;
stream_select($fin, $none, $none, 2);
var_dump(stream_get_meta_data($acc)['eof']);
var_dump(feof($acc));
var_dump(stream_get_meta_data($acc)['eof']);
/* A bound socket that never listened is already at an end on Linux -- the
 * stack reports it readable and the peek that follows finds nothing -- and is
 * not on macOS or Windows, where an unconnected socket is simply never
 * readable. A LISTENING socket and a bound DATAGRAM one are at no end on any. */
$bound = stream_socket_server('tcp://127.0.0.1:0', $e, $es, STREAM_SERVER_BIND);
$dg = stream_socket_server('udp://127.0.0.1:0', $e, $es, STREAM_SERVER_BIND);
var_dump(feof($bound) === $linux, feof($srv), feof($dg));
/* WHICH domain can be paired is the OS's answer, not php's: POSIX has AF_UNIX
 * and Windows has AF_INET, so a program that wants a pair asks for both. */
set_error_handler($mute);
$pair = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, 0);
if ($pair === false) {
    $pair = stream_socket_pair(STREAM_PF_INET, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);
}
set_error_handler($say);
fclose($pair[1]);
var_dump(feof($pair[0]));
fclose($pair[0]); fclose($acc); fclose($bound); fclose($dg);

/* (7) A PERSISTENT connection is not handed back unseen: php runs the same
 * probe and re-dials a socket the far end has finished with. */
$p1 = stream_socket_client("tcp://$nm", $e, $es, null, STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
$a1 = stream_socket_accept($srv, 1);
$p2 = stream_socket_client("tcp://$nm", $e, $es, null, STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
var_dump((int)$p1 === (int)$p2, get_resource_type($p2));
fclose($a1);
$p3 = stream_socket_client("tcp://$nm", $e, $es, null, STREAM_CLIENT_CONNECT | STREAM_CLIENT_PERSISTENT);
var_dump(is_resource($p3), (int)$p3 === (int)$p1);
$a2 = stream_socket_accept($srv, 1);
var_dump(fwrite($p3, 'fresh'), fread($a2, 8));
fclose($a2); fclose($p3); fclose($srv);

/* (8) stream_socket_sendto() reads a DIFFERENT colon rule from the one the
 * connect address uses: it searches the whole string, so a trailing colon is a
 * port of 0 rather than an address it could not parse. */
$us = stream_socket_server('udp://127.0.0.1:0', $e, $es, STREAM_SERVER_BIND);
$un = stream_socket_get_name($us, false);
$uc = stream_socket_client("udp://$un", $e, $es);
/* Port 0 is an address the two stacks answer differently -- POSIX refuses it
 * (EINVAL) and Winsock sends -- so only the PARSE is pinned: the address was
 * read, which is what separates it from the refusal below. */
set_error_handler($mute);
var_dump(@stream_socket_sendto($uc, 'p', 0, '127.0.0.1:') === ($win ? 1 : -1));
set_error_handler($say);
var_dump(@stream_socket_sendto($uc, 'p', 0, '127.0.0.1'));
var_dump(@stream_socket_client('tcp://127.0.0.1:', $e, $es), $es);
fclose($uc); fclose($us);
?>
--EXPECT--
tcp: <the stack refused the port>
TCP: Unable to find the socket transport "TCP" - did you forget to enable it when you configured PHP?
Tcp: Unable to find the socket transport "Tcp" - did you forget to enable it when you configured PHP?
udp: <no refusal: a datagram socket connects to anything>
UDP: Unable to find the socket transport "UDP" - did you forget to enable it when you configured PHP?
bool(true)
int(0)
string(0) ""
bool(true)
int(0)
string(0) ""
  ERR[2] stream_socket_server(): Unable to connect to nope://1.2.3.4:5 (Unable to find the socket transport "nope" - did you forget to enable it when you configured PHP?)
bool(false)
string(97) "Unable to find the socket transport "nope" - did you forget to enable it when you configured PHP?"
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
string(0) ""
bool(true)
bool(true)
bool(false)
bool(true)
int(0)
string(0) ""
bool(true)
bool(true)
int(5)
string(5) "async"
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
string(17) "persistent stream"
bool(true)
bool(false)
int(5)
string(5) "fresh"
bool(true)
  ERR[2] stream_socket_sendto(): Failed to parse `127.0.0.1' into a valid network address
bool(false)
  ERR[2] stream_socket_client(): Unable to connect to tcp://127.0.0.1: (Failed to parse address "127.0.0.1:")
bool(false)
string(36) "Failed to parse address "127.0.0.1:""
--CLEAN--
<?php
unset($a, $b, $c, $d, $f, $g, $as, $aa, $srv, $cli, $acc, $bound, $dg, $pair, $p1, $p2, $p3, $a1, $a2, $us, $uc, $nm, $un, $e, $es, $t, $win, $say, $mute);
