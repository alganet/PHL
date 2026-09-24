--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the `socket` context options a tcp transport applies
--SKIPIF--
<?php
if (!function_exists('stream_socket_server')) { echo "skip no socket support"; }
?>
--FILE--
<?php
$srv  = stream_socket_server('tcp://127.0.0.1:0');
$peer = stream_socket_get_name($srv, false);

/* `bindto` is the LOCAL address a client connects out from — the way a program
 * picks its interface, or a fixed source port. Asking for one and reading it
 * back is the only observable proof it reached the socket. */
$bound = false;
for ($i = 0; $i < 8 && !$bound; $i++) {
    $port = 34500 + ((getmypid() + $i * 37) % 900);
    $c  = stream_context_create(['socket' => ['bindto' => "127.0.0.1:$port"]]);
    $cl = @stream_socket_client("tcp://$peer", $e, $es, 2, STREAM_CLIENT_CONNECT, $c);
    if (is_resource($cl)) {
        $bound = stream_socket_get_name($cl, false) === "127.0.0.1:$port";
        fclose($cl);
    }
}
echo 'bindto took: ', var_export($bound, true), "\n";

/* A spelling with no colon is not an address at all: php performs no bind and
 * says nothing about it. */
$c = stream_context_create(['socket' => ['bindto' => 'nocolon']]);
$cl = stream_socket_client("tcp://$peer", $e, $es, 2, STREAM_CLIENT_CONNECT, $c);
echo 'no colon connects: ', var_export(is_resource($cl), true), "\n";
fclose($cl);

/* Neither failure shape stops the connection, and php tells them APART: a local
 * address it could not RESOLVE names the host, one the OS refused to BIND names
 * the whole spelling and the reason (192.0.2.1 is RFC 5737's never-assigned
 * TEST-NET-1, so no machine holds it). The reason TEXT is the OS's, so only the
 * part php composes is asserted. */
$sockBind = function ($spec) use ($peer) {
    $msg = '';
    set_error_handler(function ($n, $s) use (&$msg) { $msg = $s; return true; });
    $c  = stream_context_create(['socket' => ['bindto' => $spec]]);
    $cl = stream_socket_client("tcp://$peer", $e, $es, 2, STREAM_CLIENT_CONNECT, $c);
    restore_error_handler();
    if (is_resource($cl)) { fclose($cl); }
    return [$cl !== false, $msg];
};
[$ok, $msg] = $sockBind('no.such.host.invalid:1');
echo 'unresolvable still connects: ', var_export($ok, true),
     ' named: ', var_export(str_contains($msg, 'Invalid IP Address: no.such.host.invalid'), true), "\n";
[$ok, $msg] = $sockBind('192.0.2.1:0');
echo 'unbindable still connects: ', var_export($ok, true),
     ' named: ', var_export(str_contains($msg, "Failed to bind to '192.0.2.1:0', system said: "), true), "\n";
/* php RE-COMPOSES the address from the parts it parsed, so what it quotes is
 * canonical rather than what the script spelled. */
[$ok, $msg] = $sockBind('192.0.2.1:007');
echo 'quoted canonically: ',
     var_export(str_contains($msg, "Failed to bind to '192.0.2.1:7', system said: "), true), "\n";
/* And a local address is a NUMERIC literal: php never asks the resolver about
 * one, so a host NAME is refused where it would plainly have resolved. */
[$ok, $msg] = $sockBind('localhost:0');
echo 'a name is not an address: ',
     var_export(str_contains($msg, 'Invalid IP Address: localhost'), true), "\n";

/* A value that is not a STRING is the one option failure php reports as a
 * failed CONNECT rather than as a warning it carries on past. */
$c = stream_context_create(['socket' => ['bindto' => 12345]]);
$cl = @stream_socket_client("tcp://$peer", $e, $es, 2, STREAM_CLIENT_CONNECT, $c);
echo 'non-string bindto: ', var_export($cl, true), ' ', var_export($es, true), "\n";

/* so_reuseport is what lets a SECOND server take a port a first one is already
 * listening on. The CONTRAST is a POSIX one: Windows' SO_REUSEADDR — which
 * every server socket here sets, as php's does — already permits that second
 * bind, so the plain one is only asserted refused where the OS refuses it. */
$c = stream_context_create(['socket' => ['so_reuseport' => true, 'backlog' => 5,
                                         'tcp_nodelay' => true]]);
$a  = stream_socket_server('tcp://127.0.0.1:0', $e, $es,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $c);
$an = stream_socket_get_name($a, false);
$b  = @stream_socket_server("tcp://$an", $e2, $es2,
        STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $c);
echo 'reuseport second bind: ', var_export(is_resource($b), true), "\n";
$d = @stream_socket_server("tcp://$an", $e3, $es3);
echo 'contrast holds: ',
    var_export(DIRECTORY_SEPARATOR === '/' ? !is_resource($d) : true, true), "\n";

if (is_resource($b)) { fclose($b); }
if (is_resource($d)) { fclose($d); }
fclose($a);
fclose($srv);
?>
--EXPECT--
bindto took: true
no colon connects: true
unresolvable still connects: true named: true
unbindable still connects: true named: true
quoted canonically: true
a name is not an address: true
non-string bindto: false 'local_addr context option is not a string.'
reuseport second bind: true
contrast holds: true
