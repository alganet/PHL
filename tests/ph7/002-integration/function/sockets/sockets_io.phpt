--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/sockets: the read/write verbs, the two read modes, select() and the options
--SKIPIF--
<?php
if (!extension_loaded('sockets')) {
    die("skip this build has no ext/sockets\n");
}
--FILE--
<?php
/* A connected pair over the LOOPBACK rather than socket_create_pair(): which
 * $domain a pair accepts is the platform's answer and not php's (POSIX has
 * AF_UNIX and refuses AF_INET, Windows is the exact reverse), so a test that
 * has to give one answer on both builds its pair the portable way. */
function pair(): array {
    $l = socket_create_listen(0);
    socket_getsockname($l, $addr, $port);
    $c = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    socket_connect($c, '127.0.0.1', $port);
    $s = socket_accept($l);
    socket_close($l);
    return [$c, $s];
}
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    /* Both line breaks are escaped: a raw \r left in an --EXPECT-- block is
     * eaten by the runner's line handling. */
    printf("%-40s %s\n", $l, str_replace(["\r", "\n"], ['\\r', '\\n'], var_export($r, true)));
}
set_error_handler(function (int $no, string $msg): bool {
    if (error_reporting() & $no) { printf("  [%d] %s\n", $no, $msg); }
    return true;
});

echo "-- write/read is bytes in, bytes out, with no framing\n";
[$a, $b] = pair();
t('write answers what it sent',   fn() => socket_write($a, 'hello world'));
t('read takes a PREFIX',          fn() => socket_read($b, 5));
t('...and leaves the rest',       fn() => socket_read($b, 100));
t('$length caps the write',       fn() => socket_write($a, 'abcdef', 3));
t('...so only that much arrives', fn() => socket_read($b, 100));
t('an empty write is 0 bytes',    fn() => socket_write($a, ''));
t('...and so is $length = 0',     fn() => socket_write($a, 'abc', 0));

echo "-- send/recv are the same two calls with a flags word\n";
t('send answers what it sent',    fn() => socket_send($a, 'xyz', 3, 0));
t('recv fills its out parameter', function () use ($b) {
    $buf = null; $n = socket_recv($b, $buf, 10, 0); return [$n, $buf];
});
t('$length over the string is clamped', fn() => socket_send($a, 'abc', 100, 0));
t('MSG_PEEK leaves the bytes',    function () use ($b) {
    $buf = null; $n = socket_recv($b, $buf, 10, MSG_PEEK); return [$n, $buf];
});
t('...so the next recv sees them again', function () use ($b) {
    $buf = null; $n = socket_recv($b, $buf, 10, 0); return [$n, $buf];
});

echo "-- PHP_NORMAL_READ stops after the terminator, and KEEPS it\n";
[$c, $d] = pair();
socket_write($c, "line1\nline2\r\nline3");
t('the first line, with its \n',  fn() => socket_read($d, 100, PHP_NORMAL_READ));
t('the second, up to its \r',     fn() => socket_read($d, 100, PHP_NORMAL_READ));
t('...and the \n that followed',  fn() => socket_read($d, 100, PHP_NORMAL_READ));
t('a line longer than $length is cut', fn() => socket_read($d, 3, PHP_NORMAL_READ));
t('...and the rest is still there',    fn() => socket_read($d, 2, PHP_NORMAL_READ));

echo "-- an orderly close is a 0-length read, and recv answers NULL for it\n";
[$e, $f] = pair();
socket_close($e);
t('read answers the empty string', fn() => socket_read($f, 10));
t('recv answers 0 and null',       function () use ($f) {
    $buf = 'kept'; $n = socket_recv($f, $buf, 10, 0); return [$n, $buf];
});

echo "-- shutdown closes ONE direction, and a write into a dead peer fails\n";
[$g, $h] = pair();
t('shutting down the write side', fn() => socket_shutdown($g, SHUT_WR));
t('the peer sees end of stream',  fn() => socket_read($h, 10));
t('...but can still answer',      fn() => socket_write($h, 'back'));
t('...and that arrives',          fn() => socket_read($g, 10));
socket_close($h);
t('a write with nobody there',    function () use ($g) {
    socket_clear_error();
    $r = @socket_write($g, 'x');
    return [$r, socket_last_error($g) !== 0];
});

echo "-- socket_select rebuilds each array KEYS AND ALL\n";
[$i, $j] = pair();
[$k, $m] = pair();
socket_write($i, 'x');
t('only the readable one is kept', function () use ($j, $m) {
    $r = ['ready' => $j, 'quiet' => $m, 7 => $j];
    $w = null; $e = null;
    /* a moment's wait: the byte written above need not have crossed the
     * loopback yet when a zero-timeout poll looks */
    $n = socket_select($r, $w, $e, 1, 0);
    return [$n, array_keys($r)];
});
t('both are writable',            function () use ($i, $k) {
    $r = null; $w = [$i, $k]; $e = null;
    $n = socket_select($r, $w, $e, 0, 0);
    return [$n, count($w)];
});
t('a null array is left null',    function () use ($i) {
    $r = null; $w = [$i]; $e = null;
    socket_select($r, $w, $e, 0, 0);
    return $r;
});
t('an EMPTY array counts for nothing but is still rebuilt', function () use ($i) {
    $r = []; $w = [$i]; $e = null;
    $n = socket_select($r, $w, $e, 0, 0);
    return [$n, $r];
});
t('$microseconds over a second carries', function () use ($m) {
    $r = [$m]; $w = null; $e = null;
    return socket_select($r, $w, $e, 0, 1500);
});

echo "-- blocking mode is a property of the DESCRIPTOR, and both verbs answer true\n";
[$n1, $n2] = pair();
t('set_nonblock',                 fn() => socket_set_nonblock($n2));
t('a read with nothing waiting',  function () use ($n2) {
    socket_clear_error();
    $buf = 'kept'; $r = @socket_recv($n2, $buf, 10, 0);
    /* EAGAIN is the one failure php stores WITHOUT a warning. */
    return [$r, $buf, socket_last_error($n2) !== 0];
});
t('set_block',                    fn() => socket_set_block($n2));

echo "-- the option table: the two structs, the plain ints, and a name nothing has\n";
$o = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
t('SO_LINGER round trip',         function () use ($o) {
    socket_set_option($o, SOL_SOCKET, SO_LINGER, ['l_onoff' => 1, 'l_linger' => 3]);
    return socket_get_option($o, SOL_SOCKET, SO_LINGER);
});
t('SO_RCVTIMEO keeps its seconds', function () use ($o) {
    socket_set_option($o, SOL_SOCKET, SO_RCVTIMEO, ['sec' => 2, 'usec' => 0]);
    return socket_get_option($o, SOL_SOCKET, SO_RCVTIMEO);
});
t('SO_TYPE reports the socket',   fn() => socket_get_option($o, SOL_SOCKET, SO_TYPE) === SOCK_STREAM);
t('SO_REUSEADDR is a plain int',  function () use ($o) {
    socket_set_option($o, SOL_SOCKET, SO_REUSEADDR, 1);
    return socket_get_option($o, SOL_SOCKET, SO_REUSEADDR) !== 0;
});
t('an option nothing knows',      function () use ($o) {
    socket_clear_error();
    $r = @socket_get_option($o, SOL_SOCKET, 99999);
    return [$r, socket_last_error($o) !== 0];
});
t('the two aliases are the same function', fn() => [
    socket_getopt($o, SOL_SOCKET, SO_TYPE) === socket_get_option($o, SOL_SOCKET, SO_TYPE),
    socket_setopt($o, SOL_SOCKET, SO_REUSEADDR, 1),
]);

echo "-- socket_atmark is false on a stream with no urgent data\n";
/* POSIX only: php builds no socket_atmark for Windows. */
t('atmark', fn() => function_exists('socket_atmark') ? socket_atmark($n1) : false);
--EXPECT--
-- write/read is bytes in, bytes out, with no framing
write answers what it sent               11
read takes a PREFIX                      'hello'
...and leaves the rest                   ' world'
$length caps the write                   3
...so only that much arrives             'abc'
an empty write is 0 bytes                0
...and so is $length = 0                 0
-- send/recv are the same two calls with a flags word
send answers what it sent                3
recv fills its out parameter             array (\n  0 => 3,\n  1 => 'xyz',\n)
$length over the string is clamped       3
MSG_PEEK leaves the bytes                array (\n  0 => 3,\n  1 => 'abc',\n)
...so the next recv sees them again      array (\n  0 => 3,\n  1 => 'abc',\n)
-- PHP_NORMAL_READ stops after the terminator, and KEEPS it
the first line, with its \n              'line1\n'
the second, up to its \r                 'line2\r'
...and the \n that followed              '\n'
a line longer than $length is cut        'lin'
...and the rest is still there           'e3'
-- an orderly close is a 0-length read, and recv answers NULL for it
read answers the empty string            ''
recv answers 0 and null                  array (\n  0 => 0,\n  1 => NULL,\n)
-- shutdown closes ONE direction, and a write into a dead peer fails
shutting down the write side             true
the peer sees end of stream              ''
...but can still answer                  4
...and that arrives                      'back'
a write with nobody there                array (\n  0 => false,\n  1 => true,\n)
-- socket_select rebuilds each array KEYS AND ALL
only the readable one is kept            array (\n  0 => 1,\n  1 => \n  array (\n    0 => 'ready',\n    1 => 7,\n  ),\n)
both are writable                        array (\n  0 => 2,\n  1 => 2,\n)
a null array is left null                NULL
an EMPTY array counts for nothing but is still rebuilt array (\n  0 => 1,\n  1 => \n  array (\n  ),\n)
$microseconds over a second carries      0
-- blocking mode is a property of the DESCRIPTOR, and both verbs answer true
set_nonblock                             true
a read with nothing waiting              array (\n  0 => false,\n  1 => NULL,\n  2 => true,\n)
set_block                                true
-- the option table: the two structs, the plain ints, and a name nothing has
SO_LINGER round trip                     array (\n  'l_onoff' => 1,\n  'l_linger' => 3,\n)
SO_RCVTIMEO keeps its seconds            array (\n  'sec' => 2,\n  'usec' => 0,\n)
SO_TYPE reports the socket               true
SO_REUSEADDR is a plain int              true
an option nothing knows                  array (\n  0 => false,\n  1 => true,\n)
the two aliases are the same function    array (\n  0 => true,\n  1 => true,\n)
-- socket_atmark is false on a stream with no urgent data
atmark                                   false
