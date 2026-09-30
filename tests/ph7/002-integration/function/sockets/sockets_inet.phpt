--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/sockets: listen/connect/accept, datagrams, sendmsg, getaddrinfo and the stream doors
--SKIPIF--
<?php
if (!extension_loaded('sockets')) {
    die("skip this build has no ext/sockets\n");
}
--FILE--
<?php
/* Every PORT here is one the kernel picked, and every ADDRESS is the loopback,
 * so nothing prints a number the box owns: the answers are compared against
 * each other instead. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-44s %s\n", $l, str_replace(["\r", "\n"], ['\r', '\n'], var_export($r, true)));
}
set_error_handler(function (int $no, string $msg): bool {
    if (error_reporting() & $no) { printf("  [%d] %s\n", $no, $msg); }
    return true;
});

echo "-- a listener, a client, and the four names between them\n";
$l = socket_create_listen(0);
t('a listener IS a Socket',       fn() => get_debug_type($l));
$la = $lp = null;
t('...bound to every address',    function () use ($l, &$la, &$lp) {
    return [socket_getsockname($l, $la, $lp), $la, $lp > 0];
});
$c = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
t('a client with no peer yet',    function () use ($c) {
    socket_clear_error();
    $a = $p = null;
    return [@socket_getpeername($c, $a, $p), $a, $p, socket_last_error($c) !== 0];
});
t('connect',                      fn() => socket_connect($c, '127.0.0.1', $lp));
$s = socket_accept($l);
t('accept answers a new Socket',  fn() => get_debug_type($s));
t('the client names the server',  function () use ($c, $lp) {
    $a = $p = null;
    return [socket_getpeername($c, $a, $p), $a, $p === $lp];
});
t('...and the server names it back', function () use ($s, $c) {
    $sa = $sp = $ca = $cp = null;
    socket_getpeername($s, $sa, $sp);
    socket_getsockname($c, $ca, $cp);
    return [$sa === $ca, $sp === $cp];
});
t('the accepted socket carries the listener\'s type',
    fn() => socket_get_option($s, SOL_SOCKET, SO_TYPE) === SOCK_STREAM);
t('a second connect on a live socket', function () use ($c, $lp) {
    socket_clear_error();
    return [@socket_connect($c, '127.0.0.1', $lp), socket_last_error($c) !== 0];
});

echo "-- a datagram round trip carries the SENDER back\n";
$u1 = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
socket_bind($u1, '127.0.0.1', 0);
$ua = $up = null;
socket_getsockname($u1, $ua, $up);
$u2 = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
socket_bind($u2, '127.0.0.1', 0);
$va = $vp = null;
socket_getsockname($u2, $va, $vp);
t('sendto answers the byte count', fn() => socket_sendto($u2, 'ping', 4, 0, '127.0.0.1', $up));
t('recvfrom names the sender',     function () use ($u1, $vp) {
    $b = $a = $p = null;
    $n = socket_recvfrom($u1, $b, 10, 0, $a, $p);
    return [$n, $b, $a, $p === $vp];
});
t('a name nothing resolves is a lookup failure', function () use ($u2) {
    socket_clear_error();
    $r = @socket_sendto($u2, 'x', 1, 0, 'no.such.host.invalid.example', 9);
    /* php carries a lookup failure BELOW -10000 so it cannot collide with an
     * errno; which one it is, is the resolver's. */
    return [$r, socket_last_error($u2) <= -10000];
});

echo "-- sendmsg/recvmsg see the same bytes, gathered and scattered\n";
t('sendmsg gathers its iov',       fn() => socket_sendmsg($u2, [
    'name' => ['addr' => '127.0.0.1', 'port' => $up], 'iov' => ['a', 'bb', 'ccc']], 0));
t('recvmsg reports one buffer',    function () use ($u1) {
    $m = ['buffer_size' => 20, 'controllen' => 32];
    $n = socket_recvmsg($u1, $m, 0);
    return [$n, $m['name'], $m['control'], $m['iov'], $m['flags']];
});
t('...and NAMES the sender only when asked', function () use ($u1, $u2, $up, $vp) {
    socket_sendmsg($u2, ['name' => ['addr' => '127.0.0.1', 'port' => $up], 'iov' => ['x']], 0);
    $m = ['name' => [], 'buffer_size' => 20, 'controllen' => 32];
    socket_recvmsg($u1, $m, 0);
    return [$m['name']['addr'], $m['name']['port'] === $vp];
});
t('controllen is required',        function () use ($u1) {
    socket_clear_error();
    $m = ['buffer_size' => 10];
    return @socket_recvmsg($u1, $m, 0);
});
t('...and may not be 0',           function () use ($u1) {
    $m = ['buffer_size' => 10, 'controllen' => 0];
    return @socket_recvmsg($u1, $m, 0);
});
t('an iov that is not an array',   function () use ($u2) {
    return @socket_sendmsg($u2, ['iov' => 'a'], 0);
});

echo "-- socket_addrinfo_lookup, and the two openers that read one\n";
$ai = socket_addrinfo_lookup('127.0.0.1', '80', ['ai_family' => AF_INET, 'ai_socktype' => SOCK_STREAM]);
t('one candidate, and it is opaque', fn() => [count($ai), get_debug_type($ai[0])]);
t('explain', function () use ($ai) {
    /* `ai_flags` and `ai_protocol` are what the PLATFORM's getaddrinfo() filled
     * in for a numeric host, and the two resolvers answer differently; the
     * family, the type and the address are the contract. */
    $e = socket_addrinfo_explain($ai[0]);
    return [array_keys($e), $e['ai_family'], $e['ai_socktype'], $e['ai_addr'],
        is_int($e['ai_flags']), is_int($e['ai_protocol'])];
});
t('a canonical name only when asked', function () use ($ai) {
    $r = socket_addrinfo_lookup('127.0.0.1', '80', ['ai_family' => AF_INET,
        'ai_socktype' => SOCK_STREAM, 'ai_flags' => AI_CANONNAME]);
    $e = socket_addrinfo_explain($r[0]);
    /* a numeric host's canonical name is the resolver's call: glibc answers
     * one, macOS answers NULL and php then leaves the key out */
    return [!array_key_exists('ai_canonname', $e) || is_string($e['ai_canonname']),
        array_key_exists('ai_canonname', socket_addrinfo_explain($ai[0]))];
});
t('bind through one',              function () {
    $r = socket_addrinfo_lookup('127.0.0.1', '0', ['ai_family' => AF_INET,
        'ai_socktype' => SOCK_STREAM, 'ai_flags' => AI_PASSIVE]);
    $sock = socket_addrinfo_bind($r[0]);
    $a = $p = null;
    socket_getsockname($sock, $a, $p);
    return [get_debug_type($sock), $a, $p > 0];
});
t('connect through one',           function () use ($lp) {
    $r = socket_addrinfo_lookup('127.0.0.1', (string) $lp, ['ai_family' => AF_INET,
        'ai_socktype' => SOCK_STREAM]);
    return get_debug_type(socket_addrinfo_connect($r[0]));
});
t('a service nothing knows',       fn() => socket_addrinfo_lookup('127.0.0.1', 'no-such-service-8a1f'));

echo "-- export/import: one descriptor, two faces\n";
$x = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
$st = socket_export_stream($x);
t('a stream comes back',           fn() => get_debug_type($st));
t('...labelled by the TRANSPORT',  fn() => [
    stream_get_meta_data($st)['stream_type'],
    stream_get_meta_data($st)['uri'],
    stream_get_meta_data($st)['mode'],
]);
t('a datagram socket is labelled apart', function () {
    $d = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    return stream_get_meta_data(socket_export_stream($d))['stream_type'];
});
t('exporting twice answers the SAME handle', fn() => socket_export_stream($x) === $st);
t('importing it back gives a Socket', fn() => get_debug_type(socket_import_stream($st)));
t('a stream with no descriptor cannot be one', function () {
    socket_clear_error();
    return @socket_import_stream(fopen('php://memory', 'r'));
});
t('bytes written through the stream arrive at the socket', function () {
    $l2 = socket_create_listen(0);
    $a2 = $p2 = null;
    socket_getsockname($l2, $a2, $p2);
    $c2 = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_connect($c2, '127.0.0.1', $p2);
    $s2 = socket_accept($l2);
    fwrite(socket_export_stream($c2), 'through the stream');
    return socket_read($s2, 100);
});
t('...and the other way round',    function () {
    $l3 = socket_create_listen(0);
    $a3 = $p3 = null;
    socket_getsockname($l3, $a3, $p3);
    $c3 = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_connect($c3, '127.0.0.1', $p3);
    $s3 = socket_accept($l3);
    socket_write($c3, 'through the socket');
    return fread(socket_export_stream($s3), 100);
});
--EXPECT--
-- a listener, a client, and the four names between them
a listener IS a Socket                       'Socket'
...bound to every address                    array (\n  0 => true,\n  1 => '0.0.0.0',\n  2 => true,\n)
a client with no peer yet                    array (\n  0 => false,\n  1 => NULL,\n  2 => NULL,\n  3 => true,\n)
connect                                      true
accept answers a new Socket                  'Socket'
the client names the server                  array (\n  0 => true,\n  1 => '127.0.0.1',\n  2 => true,\n)
...and the server names it back              array (\n  0 => true,\n  1 => true,\n)
the accepted socket carries the listener's type true
a second connect on a live socket            array (\n  0 => false,\n  1 => true,\n)
-- a datagram round trip carries the SENDER back
sendto answers the byte count                4
recvfrom names the sender                    array (\n  0 => 4,\n  1 => 'ping',\n  2 => '127.0.0.1',\n  3 => true,\n)
a name nothing resolves is a lookup failure  array (\n  0 => false,\n  1 => true,\n)
-- sendmsg/recvmsg see the same bytes, gathered and scattered
sendmsg gathers its iov                      6
recvmsg reports one buffer                   array (\n  0 => 6,\n  1 => NULL,\n  2 => \n  array (\n  ),\n  3 => \n  array (\n    0 => 'abbccc',\n  ),\n  4 => 0,\n)
...and NAMES the sender only when asked      array (\n  0 => '127.0.0.1',\n  1 => true,\n)
controllen is required                       false
...and may not be 0                          false
an iov that is not an array                  false
-- socket_addrinfo_lookup, and the two openers that read one
one candidate, and it is opaque              array (\n  0 => 1,\n  1 => 'AddressInfo',\n)
explain                                      array (\n  0 => \n  array (\n    0 => 'ai_flags',\n    1 => 'ai_family',\n    2 => 'ai_socktype',\n    3 => 'ai_protocol',\n    4 => 'ai_addr',\n  ),\n  1 => 2,\n  2 => 1,\n  3 => \n  array (\n    'sin_port' => 80,\n    'sin_addr' => '127.0.0.1',\n  ),\n  4 => true,\n  5 => true,\n)
a canonical name only when asked             array (\n  0 => true,\n  1 => false,\n)
bind through one                             array (\n  0 => 'Socket',\n  1 => '127.0.0.1',\n  2 => true,\n)
connect through one                          'Socket'
a service nothing knows                      false
-- export/import: one descriptor, two faces
a stream comes back                          'resource (stream)'
...labelled by the TRANSPORT                 array (\n  0 => 'tcp_socket/ssl',\n  1 => 'tcp://',\n  2 => 'r+',\n)
a datagram socket is labelled apart          'udp_socket'
exporting twice answers the SAME handle      true
importing it back gives a Socket             'Socket'
a stream with no descriptor cannot be one    false
bytes written through the stream arrive at the socket 'through the stream'
...and the other way round                   'through the socket'
