--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the stream filter chain — append, prepend, remove, and which chain
--DESCRIPTION--
stream_filter_append/prepend/remove and stream_get_filters were each a loud Call
to undefined function, so nothing could be put between a stream and the bytes it
carries. This is the chain: what a filter is attached TO (php's $mode of 0 is not
"neither" — it is "whichever chains the handle's own mode makes sense for", so an
r+ handle gets two separate instances), the ORDER a chain runs in, and what
removing one does (it flushes, and the resource is dead afterwards).
--FILE--
<?php
$sfltShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
};
$sfltFile = tempnam(sys_get_temp_dir(), 'sflt');
$sfltPut = function ($s) use ($sfltFile) { file_put_contents($sfltFile, $s); return $sfltFile; };

/* It is a resource, and php names it with a SPACE where the context has a dash. */
$sfltPut("Hello World\n");
$sfltH = fopen($sfltFile, 'r');
$sfltF = stream_filter_append($sfltH, 'string.toupper');
$sfltShow('is_resource', fn() => is_resource($sfltF));
$sfltShow('type', fn() => get_resource_type($sfltF));
$sfltShow('debug_type', fn() => get_debug_type($sfltF));
$sfltShow('read through it', fn() => fread($sfltH, 5));
$sfltShow('and the line reader too', fn() => fgets($sfltH));
fclose($sfltH);
$sfltShow('closing the stream closes it', fn() => is_resource($sfltF));

/* A name nothing registered is a warning and FALSE, and the match is CASE
 * sensitive: php has no STRING.ROT13. */
$sfltShow('unknown name', function () use ($sfltFile) {
    $h = fopen($sfltFile, 'r');
    $e = 'none';
    set_error_handler(function ($n, $m) use (&$e) { $e = $m; return true; });
    $r = stream_filter_append($h, 'nope.nope');
    restore_error_handler();
    fclose($h);
    return [$r, $e];
});
$sfltShow('wrong case', function () use ($sfltFile) {
    $h = fopen($sfltFile, 'r');
    $r = @stream_filter_append($h, 'STRING.ROT13');
    fclose($h);
    return $r;
});

/* The write chain runs before the device sees anything, and fwrite() still
 * answers what it CONSUMED. */
$sfltShow('write chain', function () use ($sfltFile) {
    $h = fopen($sfltFile, 'w');
    stream_filter_append($h, 'string.toupper', STREAM_FILTER_WRITE);
    $n = [fwrite($h, 'abc'), fwrite($h, 'defgh')];
    fclose($h);
    return [$n, file_get_contents($sfltFile)];
});

/* Order: append goes to the tail, prepend to the head, so both of these run
 * toupper first and rot13 second. */
$sfltShow('append order', function () use ($sfltFile) {
    $h = fopen($sfltFile, 'w');
    stream_filter_append($h, 'string.toupper', STREAM_FILTER_WRITE);
    stream_filter_append($h, 'string.rot13', STREAM_FILTER_WRITE);
    fwrite($h, 'abc');
    fclose($h);
    return file_get_contents($sfltFile);
});
$sfltShow('prepend order', function () use ($sfltFile) {
    $h = fopen($sfltFile, 'w');
    stream_filter_append($h, 'string.rot13', STREAM_FILTER_WRITE);
    stream_filter_prepend($h, 'string.toupper', STREAM_FILTER_WRITE);
    fwrite($h, 'abc');
    fclose($h);
    return file_get_contents($sfltFile);
});
$sfltShow('tolower', function () use ($sfltPut) {
    $h = fopen($sfltPut('MiXeD'), 'r');
    stream_filter_append($h, 'string.tolower');
    $s = fread($h, 16);
    fclose($h);
    return $s;
});

/* $mode 0 reads the handle's own mode: r is read-only, w is write-only, and r+
 * is BOTH — two instances, of which the resource answered is the write one, so
 * removing it leaves the read half still filtering. */
$sfltShow('mode 0 on r+', function () use ($sfltPut) {
    $p = $sfltPut('abc');
    $h = fopen($p, 'r+');
    stream_filter_append($h, 'string.toupper');
    $read = fread($h, 3);
    fseek($h, 0);
    fwrite($h, 'xyz');
    fclose($h);
    return [$read, file_get_contents($p)];
});
$sfltShow('mode 0 on w does not filter reads', function () use ($sfltPut) {
    $h = fopen($sfltPut('abc'), 'r');
    $f = stream_filter_append($h, 'string.toupper', STREAM_FILTER_WRITE);
    $s = fread($h, 3);
    fclose($h);
    return $s;
});

/* Removing flushes and then kills the resource: a second removal is a TypeError,
 * not FALSE. */
$sfltShow('remove', function () use ($sfltPut) {
    $h = fopen($sfltPut('abcdefgh'), 'r');
    $f = stream_filter_append($h, 'string.toupper');
    $first = fread($h, 2);
    $ok = stream_filter_remove($f);
    $live = is_resource($f);
    fclose($h);
    return [$first, $ok, $live];
});
$sfltShow('remove twice', function () use ($sfltPut) {
    $h = fopen($sfltPut('abc'), 'r');
    $f = stream_filter_append($h, 'string.toupper');
    stream_filter_remove($f);
    try { return stream_filter_remove($f); }
    finally { fclose($h); }
});
$sfltShow('remove something else', fn() => stream_filter_remove(STDIN));
$sfltShow('remove a non-resource', fn() => stream_filter_remove('x'));

/* What the chain has already produced sits AHEAD of where the script is, so
 * ftell(), a SEEK_CUR seek and `unread_bytes` all discount it. */
$sfltShow('position under a filter', function () use ($sfltPut) {
    $h = fopen($sfltPut('0123456789'), 'r');
    stream_filter_append($h, 'string.rot13');
    $first = fread($h, 3);
    $tell = ftell($h);
    fseek($h, 0, SEEK_CUR);
    $next = fread($h, 3);
    $unread = stream_get_meta_data($h)['unread_bytes'];
    fclose($h);
    return [$first, $tell, $next, $unread];
});

/* The chain selectors. */
$sfltShow('selectors', fn() => [STREAM_FILTER_READ, STREAM_FILTER_WRITE, STREAM_FILTER_ALL]);

/* The names this build can create. php's list carries the extensions it was
 * built with (zlib.*, convert.iconv.*), so only the ones in scope are asserted. */
$sfltShow('string filters registered', fn() => array_values(array_filter(
    stream_get_filters(), fn($n) => str_starts_with($n, 'string.'))));

@unlink($sfltFile);
?>
--EXPECT--
is_resource => true
type => 'stream filter'
debug_type => 'resource (stream filter)'
read through it => 'HELLO'
and the line reader too => ' WORLD'
closing the stream closes it => false
unknown name => array (  0 => false,  1 => 'stream_filter_append(): Unable to locate filter "nope.nope"',)
wrong case => false
write chain => array (  0 =>   array (    0 => 3,    1 => 5,  ),  1 => 'ABCDEFGH',)
append order => 'NOP'
prepend order => 'NOP'
tolower => 'mixed'
mode 0 on r+ => array (  0 => 'ABC',  1 => 'XYZ',)
mode 0 on w does not filter reads => 'abc'
remove => array (  0 => 'AB',  1 => true,  2 => false,)
remove twice => TypeError: stream_filter_remove(): supplied resource is not a valid stream filter resource
remove something else => TypeError: stream_filter_remove(): supplied resource is not a valid stream filter resource
remove a non-resource => TypeError: stream_filter_remove(): Argument #1 ($stream_filter) must be of type resource, string given
position under a filter => array (  0 => '012',  1 => 3,  2 => '345',  3 => 4,)
selectors => array (  0 => 1,  1 => 2,  2 => 3,)
string filters registered => array (  0 => 'string.rot13',  1 => 'string.toupper',  2 => 'string.tolower',)
