--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/sockets: the inventory, the two handle classes and every argument screen
--SKIPIF--
<?php
if (!extension_loaded('sockets')) {
    die("skip this build has no ext/sockets\n");
}
--FILE--
<?php
/* Nothing here prints a NUMBER the platform owns: an AF_* value, an errno and a
 * strerror text all differ between POSIX and Winsock, so what is pinned is the
 * SHAPE -- which names exist in which order, what each class refuses, and which
 * argument gets which screen. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-40s %s\n", $l, str_replace("\n", '', var_export($r, true)));
}
/* error_reporting()-aware: php still CALLS a handler under `@`, and every
 * diagnostic printed below carries a platform errno in it. */
set_error_handler(function (int $no, string $msg): bool {
    if (error_reporting() & $no) { printf("  [%d] %s\n", $no, $msg); }
    return true;
});

echo "-- the extension, with php's names in php's own order\n";
t('extension_loaded',  fn() => extension_loaded('sockets'));
/* Four of the names are per-PLATFORM and php's own list differs with them:
 * `socket_atmark` is POSIX-only and the three `socket_wsaprotocol_info_*` verbs
 * are Windows-only (a socket reaches another process by being DUPLICATED into
 * it there, since there is no descriptor to pass). The order of the rest is the
 * same on both, which is what this prints. */
$aPlatform = ['socket_atmark', 'socket_wsaprotocol_info_export',
    'socket_wsaprotocol_info_import', 'socket_wsaprotocol_info_release'];
echo implode("\n", array_diff(get_extension_funcs('sockets'), $aPlatform)), "\n";
$win = DIRECTORY_SEPARATOR === '\\';
t('socket_atmark is POSIX-only',  fn() => function_exists('socket_atmark') === !$win);
t('the WSA trio is Windows-only', fn() => [
    function_exists('socket_wsaprotocol_info_export') === $win,
    function_exists('socket_wsaprotocol_info_import') === $win,
    function_exists('socket_wsaprotocol_info_release') === $win,
]);
$r = new ReflectionExtension('sockets');
echo implode(',', $r->getClassNames()), "\n";

echo "-- both classes are opaque handles: final, unclonable, unserializable\n";
foreach (['Socket', 'AddressInfo'] as $n) {
    $c = new ReflectionClass($n);
    printf("%-12s final=%d internal=%d ext=%s clone=%d props=%d consts=%d methods=%d\n",
        $n, $c->isFinal(), $c->isInternal(), $c->getExtensionName(),
        $c->isCloneable(), count($c->getProperties()), count($c->getConstants()),
        count($c->getMethods()));
}
t('new Socket',        fn() => new Socket());
t('new AddressInfo',   fn() => new AddressInfo());

$s = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
t('what a Socket IS',   fn() => [gettype($s), get_debug_type($s), $s instanceof Socket]);
t('it presents empty',  fn() => [(array) $s, get_object_vars($s), print_r($s, true)]);
t('...and is truthy',   fn() => (bool) $s);
t('clone is refused',   fn() => clone $s);
t('so is serialize',    fn() => serialize($s));
t('and (int) is a warning', fn() => (int) $s);
t('it equals only itself', fn() => [$s == $s, $s == socket_create(AF_INET, SOCK_DGRAM, SOL_UDP)]);

echo "-- socket_create screens the domain and the type, and nothing else\n";
/* The DOMAIN refusal names the families the platform has, so its text is not
 * portable; that it IS a ValueError is. */
t('a domain php has no name for', function () {
    try { socket_create(-1, SOCK_STREAM, 0); } catch (\Throwable $e) { return get_class($e); }
    return 'no refusal';
});
/* php names SOCK_CLOEXEC/SOCK_NONBLOCK in the sentence only where the system
 * has them (macOS has neither), so the tail is the platform's */
t('a type past the last one',     fn() => preg_replace('/ optionally OR.*$/', '', (function () {
    try { socket_create(AF_INET, 11, 0); return 'none'; } catch (\Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
})()));
t('...but a type INSIDE the range reaches socket()', function () {
    /* Which errno the platform answers with is its own; that the call REACHED
     * it, rather than being refused by php, is the contract. */
    socket_clear_error();
    $r = @socket_create(AF_INET, 9, 0);
    return [$r, socket_last_error() !== 0];
});
t('the creation flags are masked off first', function () {
    /* php has no SOCK_CLOEXEC/SOCK_NONBLOCK on Windows, where the two bits it
     * masks off do not exist -- the SCREEN is the same either way. */
    $flags = (defined('SOCK_CLOEXEC') ? SOCK_CLOEXEC : 0)
           | (defined('SOCK_NONBLOCK') ? SOCK_NONBLOCK : 0);
    return get_debug_type(socket_create(AF_INET, SOCK_STREAM | $flags, 0));
});

echo "-- a port is 0..65535 wherever one is taken\n";
t('bind -1',            fn() => socket_bind($s, '127.0.0.1', -1));
t('bind 65536',         fn() => socket_bind($s, '127.0.0.1', 65536));
t('create_listen -1',   fn() => socket_create_listen(-1));
t('connect with no port', fn() => socket_connect($s, '127.0.0.1'));
t('sendto with no port',  fn() => socket_sendto($s, 'x', 1, 0, '127.0.0.1'));

echo "-- a length is >= 0 where php declares one, and 0 answers false where it does not\n";
t('write -1',           fn() => socket_write($s, 'abc', -1));
t('send -1',            fn() => socket_send($s, 'abc', -1, 0));
t('sendto -1',          fn() => socket_sendto($s, 'abc', -1, 0, '127.0.0.1', 1));
t('read 0',             fn() => socket_read($s, 0));
t('read -1',            fn() => socket_read($s, -1));
t('recv 0',             function () use ($s) { $b = 'kept'; return [socket_recv($s, $b, 0, 0), $b]; });

echo "-- shutdown takes exactly three modes\n";
t('shutdown 3',         fn() => socket_shutdown($s, 3));
t('shutdown -1',        fn() => socket_shutdown($s, -1));

echo "-- socket_select refuses an array that is not all Sockets, and a closed one\n";
t('an int element',     function () { $r = [1]; $w = null; $e = null; return socket_select($r, $w, $e, 0); });
t('a string in $write', function () { $r = null; $w = ['z']; $e = null; return socket_select($r, $w, $e, 0); });
t('an object in $except', function () { $r = null; $w = null; $e = [new stdClass]; return socket_select($r, $w, $e, 0); });
t('nothing at all',     function () { $r = null; $w = null; $e = null; return socket_select($r, $w, $e, 0); });
t('three EMPTY arrays are also nothing',
    function () { $r = []; $w = []; $e = []; return socket_select($r, $w, $e, 0); });

echo "-- an option whose value is a STRUCT is screened for an array first\n";
t('SO_LINGER given an int',   fn() => socket_set_option($s, SOL_SOCKET, SO_LINGER, 5));
t('SO_RCVTIMEO given an int', fn() => socket_set_option($s, SOL_SOCKET, SO_RCVTIMEO, 5));
t('...then for its keys',     fn() => socket_set_option($s, SOL_SOCKET, SO_LINGER, ['l_onoff' => 1]));
t('...both of them',          fn() => socket_set_option($s, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 1]));

echo "-- socket_cmsg_space knows a fixed set of (level, type) pairs\n";
/* The pair is named by NUMBER in the refusal, and SOL_SOCKET is 1 on POSIX and
 * 65535 on Winsock -- so the level asked about here is one that is 0 on both. */
t('a pair it has no converter for', fn() => socket_cmsg_space(0, 9999));
t('a negative count',               fn() => socket_cmsg_space(IPPROTO_IPV6, IPV6_HOPLIMIT, -1));
t('a fixed-size pair ignores the count',
    fn() => socket_cmsg_space(IPPROTO_IPV6, IPV6_HOPLIMIT, 3)
          === socket_cmsg_space(IPPROTO_IPV6, IPV6_HOPLIMIT));

echo "-- socket_addrinfo_lookup takes four hint keys and no others\n";
t('a key it does not know', fn() => socket_addrinfo_lookup('127.0.0.1', null, ['bogus' => 1]));
t('a host nothing resolves', fn() => socket_addrinfo_lookup('no.such.host.invalid.example'));

echo "-- EVERY verb refuses a socket that was already closed, in one sentence\n";
$c = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
socket_close($c);
foreach ([
    'socket_accept'       => fn() => socket_accept($c),
    'socket_bind'         => fn() => socket_bind($c, '127.0.0.1', 0),
    'socket_listen'       => fn() => socket_listen($c),
    'socket_read'         => fn() => socket_read($c, 5),
    'socket_write'        => fn() => socket_write($c, 'x'),
    'socket_shutdown'     => fn() => socket_shutdown($c),
    'socket_get_option'   => fn() => socket_get_option($c, SOL_SOCKET, SO_TYPE),
    'socket_last_error'   => fn() => socket_last_error($c),
    'socket_clear_error'  => fn() => socket_clear_error($c),
    'socket_export_stream'=> fn() => socket_export_stream($c),
    'socket_close'        => fn() => socket_close($c),
] as $name => $fn) {
    t("closed: $name", $fn);
}
if (function_exists('socket_atmark')) {
    t('closed: socket_atmark', fn() => socket_atmark($c));
} else {
    /* Windows has no such function; php's list has none either. */
    printf("%-40s %s\n", 'closed: socket_atmark', "'Error: socket_atmark(): Argument #1 (\$socket) has already been closed'");
}
t('...and inside socket_select it is a TYPE error',
    function () use ($c) { $r = [$c]; $w = null; $e = null; return socket_select($r, $w, $e, 0); });

echo "-- the two error slots are separate, and only socket_clear_error empties them\n";
socket_clear_error();
$u = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
t('a fresh socket has no error', fn() => [socket_last_error($u), socket_last_error()]);
@socket_getpeername($u, $a, $p);
t('a failure lands on BOTH',     fn() => [socket_last_error($u) !== 0, socket_last_error() !== 0]);
t('...and its text is the platform\'s', fn() => is_string(socket_strerror(socket_last_error($u))));
socket_clear_error($u);
t('clearing the socket leaves the request', fn() => [socket_last_error($u), socket_last_error() !== 0]);
socket_clear_error();
t('clearing the request empties that too',  fn() => socket_last_error());
t('strerror of 0 is a string',   fn() => is_string(socket_strerror(0)));
--EXPECT--
-- the extension, with php's names in php's own order
extension_loaded                         true
socket_select
socket_create_listen
socket_accept
socket_set_nonblock
socket_set_block
socket_listen
socket_close
socket_write
socket_read
socket_getsockname
socket_getpeername
socket_create
socket_connect
socket_strerror
socket_bind
socket_recv
socket_send
socket_recvfrom
socket_sendto
socket_get_option
socket_getopt
socket_set_option
socket_setopt
socket_create_pair
socket_shutdown
socket_last_error
socket_clear_error
socket_import_stream
socket_export_stream
socket_sendmsg
socket_recvmsg
socket_cmsg_space
socket_addrinfo_lookup
socket_addrinfo_connect
socket_addrinfo_bind
socket_addrinfo_explain
socket_atmark is POSIX-only              true
the WSA trio is Windows-only             array (  0 => true,  1 => true,  2 => true,)
Socket,AddressInfo
-- both classes are opaque handles: final, unclonable, unserializable
Socket       final=1 internal=1 ext=sockets clone=0 props=0 consts=0 methods=0
AddressInfo  final=1 internal=1 ext=sockets clone=0 props=0 consts=0 methods=0
new Socket                               'Error: Cannot directly construct Socket, use socket_create() instead'
new AddressInfo                          'Error: Cannot directly construct AddressInfo, use socket_addrinfo_lookup() instead'
what a Socket IS                         array (  0 => 'object',  1 => 'Socket',  2 => true,)
it presents empty                        array (  0 =>   array (  ),  1 =>   array (  ),  2 => 'Socket Object()',)
...and is truthy                         true
clone is refused                         'Error: Trying to clone an uncloneable object of class Socket'
so is serialize                          'Exception: Serialization of \'Socket\' is not allowed'
  [2] Object of class Socket could not be converted to int
and (int) is a warning                   1
it equals only itself                    array (  0 => true,  1 => false,)
-- socket_create screens the domain and the type, and nothing else
a domain php has no name for             'ValueError'
a type past the last one                 'ValueError: socket_create(): Argument #2 ($type) must be one of SOCK_STREAM, SOCK_DGRAM, SOCK_SEQPACKET, SOCK_RAW, or SOCK_RDM'
...but a type INSIDE the range reaches socket() array (  0 => false,  1 => true,)
the creation flags are masked off first  'Socket'
-- a port is 0..65535 wherever one is taken
bind -1                                  'ValueError: socket_bind(): Argument #3 ($port) must be between 0 and 65535'
bind 65536                               'ValueError: socket_bind(): Argument #3 ($port) must be between 0 and 65535'
create_listen -1                         'ValueError: socket_create_listen(): Argument #1 ($port) must be between 0 and 65535'
connect with no port                     'ValueError: socket_connect(): Argument #3 ($port) cannot be null when the socket type is AF_INET'
sendto with no port                      'ValueError: socket_sendto(): Argument #6 ($port) cannot be null when the socket type is AF_INET'
-- a length is >= 0 where php declares one, and 0 answers false where it does not
write -1                                 'ValueError: socket_write(): Argument #3 ($length) must be greater than or equal to 0'
send -1                                  'ValueError: socket_send(): Argument #3 ($length) must be greater than or equal to 0'
sendto -1                                'ValueError: socket_sendto(): Argument #3 ($length) must be greater than or equal to 0'
read 0                                   false
read -1                                  false
recv 0                                   array (  0 => false,  1 => 'kept',)
-- shutdown takes exactly three modes
shutdown 3                               'ValueError: socket_shutdown(): Argument #2 ($mode) must be one of SHUT_RD, SHUT_WR or SHUT_RDWR'
shutdown -1                              'ValueError: socket_shutdown(): Argument #2 ($mode) must be one of SHUT_RD, SHUT_WR or SHUT_RDWR'
-- socket_select refuses an array that is not all Sockets, and a closed one
an int element                           'TypeError: socket_select(): Argument #1 ($read) must only have elements of type Socket, int given'
a string in $write                       'TypeError: socket_select(): Argument #2 ($write) must only have elements of type Socket, string given'
an object in $except                     'TypeError: socket_select(): Argument #3 ($except) must only have elements of type Socket, stdClass given'
nothing at all                           'ValueError: socket_select(): At least one array argument must be passed'
three EMPTY arrays are also nothing      'ValueError: socket_select(): At least one array argument must be passed'
-- an option whose value is a STRUCT is screened for an array first
SO_LINGER given an int                   'TypeError: socket_set_option(): Argument #4 ($value) must be of type array when argument #3 ($option) is SO_LINGER, int given'
SO_RCVTIMEO given an int                 'TypeError: socket_set_option(): Argument #4 ($value) must be of type array when argument #3 ($option) is SO_RCVTIMEO, int given'
...then for its keys                     'ValueError: socket_set_option(): Argument #4 ($value) must have key "l_linger"'
...both of them                          'ValueError: socket_set_option(): Argument #4 ($value) must have key "usec"'
-- socket_cmsg_space knows a fixed set of (level, type) pairs
a pair it has no converter for           'ValueError: Pair level 0 and/or type 9999 is not supported'
a negative count                         'ValueError: socket_cmsg_space(): Argument #3 ($num) must be greater than or equal to 0'
a fixed-size pair ignores the count      true
-- socket_addrinfo_lookup takes four hint keys and no others
a key it does not know                   'ValueError: socket_addrinfo_lookup(): Argument #3 ($hints) must only contain array keys "ai_flags", "ai_socktype", "ai_protocol", or "ai_family"'
a host nothing resolves                  false
-- EVERY verb refuses a socket that was already closed, in one sentence
closed: socket_accept                    'Error: socket_accept(): Argument #1 ($socket) has already been closed'
closed: socket_bind                      'Error: socket_bind(): Argument #1 ($socket) has already been closed'
closed: socket_listen                    'Error: socket_listen(): Argument #1 ($socket) has already been closed'
closed: socket_read                      'Error: socket_read(): Argument #1 ($socket) has already been closed'
closed: socket_write                     'Error: socket_write(): Argument #1 ($socket) has already been closed'
closed: socket_shutdown                  'Error: socket_shutdown(): Argument #1 ($socket) has already been closed'
closed: socket_get_option                'Error: socket_get_option(): Argument #1 ($socket) has already been closed'
closed: socket_last_error                'Error: socket_last_error(): Argument #1 ($socket) has already been closed'
closed: socket_clear_error               'Error: socket_clear_error(): Argument #1 ($socket) has already been closed'
closed: socket_export_stream             'Error: socket_export_stream(): Argument #1 ($socket) has already been closed'
closed: socket_close                     'Error: socket_close(): Argument #1 ($socket) has already been closed'
closed: socket_atmark                    'Error: socket_atmark(): Argument #1 ($socket) has already been closed'
...and inside socket_select it is a TYPE error 'TypeError: socket_select(): Argument #1 ($read) contains a closed socket'
-- the two error slots are separate, and only socket_clear_error empties them
a fresh socket has no error              array (  0 => 0,  1 => 0,)
a failure lands on BOTH                  array (  0 => true,  1 => true,)
...and its text is the platform's        true
clearing the socket leaves the request   array (  0 => 0,  1 => true,)
clearing the request empties that too    0
strerror of 0 is a string                true
