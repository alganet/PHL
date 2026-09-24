--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the convert.* codecs and dechunk
--DESCRIPTION--
php registers ONE factory for the whole `convert.` prefix and picks the codec
from the rest of the name — which is what makes the wildcard lookup matter. All
four codecs are stateful across calls, and the rules a re-derivation gets wrong
are the ones measured here: what `line-length` has to be before either encoder
wraps at all (base64 rounds it down to a whole group of four; quoted-printable
wants room for an escape plus its own soft-break `=`), that quoted-printable
only sees LINES once a length is set — so the input's own break is escaped
without one and passed through with one — and that its whitespace decision is
made with the bytes of THIS read and no more. dechunk is php's forgiving one: a
body that is not chunked comes back whole.
--FILE--
<?php
$scvShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace(["\r", "\n"], ['\r', '\n'], $out), "\n";
};
$scvRun = function ($data, $name, $params = null, $chunk = 0) {
    $h = fopen('php://memory', 'w+');
    fwrite($h, $data);
    rewind($h);
    if ($chunk > 0) { stream_set_chunk_size($h, $chunk); }
    $r = $params === null
        ? stream_filter_append($h, $name, STREAM_FILTER_READ)
        : stream_filter_append($h, $name, STREAM_FILTER_READ, $params);
    $out = $r === false ? false : stream_get_contents($h);
    fclose($h);
    return $out;
};

/* The wildcard: `convert.*` answers for every codec name behind it, and for no
 * name it has no codec for. */
$scvShow('registered names', fn() => array_values(array_filter(
    stream_get_filters(), fn($n) => $n === 'convert.*' || $n === 'dechunk')));
$scvShow('unknown convert', function () use ($scvRun) {
    set_error_handler(fn() => true);
    $r = $scvRun('x', 'convert.nosuchthing', []);
    restore_error_handler();
    return $r;
});

/* base64. A $params that is PRESENT and not an array is refused — and an
 * explicit null IS present, so it fails where omitting the argument works. */
$scvShow('encode', fn() => $scvRun('abcdefghi', 'convert.base64-encode'));
$scvShow('explicit null params', function () {
    $h = fopen('php://memory', 'w+');
    fwrite($h, 'abcdefghi');
    rewind($h);
    $msg = 'none';
    set_error_handler(function ($n, $m) use (&$msg) { $msg = $m; return true; });
    $r = stream_filter_append($h, 'convert.base64-encode', STREAM_FILTER_READ, null);
    restore_error_handler();
    fclose($h);
    return [$r, $msg];
});
$scvShow('decode', fn() => $scvRun(base64_encode('abcdefghi'), 'convert.base64-decode', []));
$scvShow('decode skips what is not in the alphabet',
    fn() => $scvRun("YWJ!!!j", 'convert.base64-decode', []));
$scvShow('decode padding closes the group',
    fn() => [$scvRun('YW=', 'convert.base64-decode', []),
             $scvRun('YWJ=', 'convert.base64-decode', []),
             $scvRun('YWJjZA==', 'convert.base64-decode', [])]);
$scvShow('base64 line lengths round down to four', fn() => array_map(
    fn($L) => $scvRun('abcdefghijkl', 'convert.base64-encode',
        ['line-length' => $L, 'line-break-chars' => "\n"]),
    [0, 3, 4, 7, 8]));

/* quoted-printable. */
$scvShow('qp encode', fn() => $scvRun("h\xc3\xa9llo=\x00world  \r\ntab\there",
    'convert.quoted-printable-encode', []));
$scvShow('qp encode binary', fn() => $scvRun("h\xc3\xa9llo=\x00world  \r\ntab\there",
    'convert.quoted-printable-encode', ['binary' => true]));
$scvShow('qp line lengths need four', fn() => array_map(
    fn($L) => $scvRun("abcdef\xffghi", 'convert.quoted-printable-encode',
        ['line-length' => $L, 'line-break-chars' => "\n"]),
    [0, 3, 4, 5, 7]));
$scvShow('qp lines only exist with a length', fn() => [
    $scvRun("a\r\nb", 'convert.quoted-printable-encode', []),
    $scvRun("a\r\nb", 'convert.quoted-printable-encode', ['line-length' => 11]),
]);
$scvShow('qp whitespace sees only this read', fn() => [
    $scvRun('x0 Ty', 'convert.quoted-printable-encode', ['line-length' => 11]),
    $scvRun('a b c', 'convert.quoted-printable-encode', ['line-length' => 11]),
    $scvRun("a \r\nb", 'convert.quoted-printable-encode', ['line-length' => 11]),
    $scvRun('x0 Ty', 'convert.quoted-printable-encode', ['line-length' => 11], 1),
]);
$scvShow('qp decode', fn() => $scvRun("h=C3=A9llo=3D=00world=20=0D=0A",
    'convert.quoted-printable-decode', []));
$scvShow('qp decode soft breaks', fn() => [
    $scvRun("abc=\r\ndef", 'convert.quoted-printable-decode', []),
    $scvRun("abc=\ndef", 'convert.quoted-printable-decode', []),
    $scvRun('trail=', 'convert.quoted-printable-decode', []),
]);

/* A codec is stateful across calls: the same answer under any chunk size. */
$scvShow('chunk size does not change a round trip', function () use ($scvRun) {
    $raw = str_repeat("\x00\x01binary\xff\xfe data ", 7);
    $out = [];
    foreach ([1, 2, 3, 17, 8192] as $c) {
        $out[] = $scvRun($scvRun($raw, 'convert.base64-encode',
            ['line-length' => 8, 'line-break-chars' => "\n"], $c),
            'convert.base64-decode', [], $c) === $raw;
    }
    return $out;
});

/* Closing ONE filter is not closing the chain: what it emits travels through
 * the rest as ordinary data, so a codec below stays mid-group. And a seek
 * re-opens a chain the old end of file had closed. */
$scvShow('removing an upstream filter', function () {
    $h = fopen('php://memory', 'w+');
    $up = stream_filter_append($h, 'string.toupper', STREAM_FILTER_WRITE);
    stream_filter_append($h, 'convert.base64-encode', STREAM_FILTER_WRITE, []);
    fwrite($h, 'abcd');
    stream_filter_remove($up);
    rewind($h);
    $s = stream_get_contents($h);
    fclose($h);
    return $s;
});
$scvShow('rewind re-opens the chain', function () {
    $h = fopen('php://memory', 'w+');
    fwrite($h, 'ab');
    rewind($h);
    stream_filter_append($h, 'convert.base64-encode', STREAM_FILTER_READ, []);
    $first = stream_get_contents($h);
    rewind($h);
    $again = stream_get_contents($h);
    fclose($h);
    return [$first, $again];
});
$scvShow('a refusal is reported per write and never at close', function () {
    $h = fopen('php://memory', 'w+');
    stream_filter_append($h, 'convert.quoted-printable-decode', STREAM_FILTER_WRITE, []);
    $seen = [];
    set_error_handler(function ($n, $m) use (&$seen) { $seen[] = $m; return true; });
    fwrite($h, 'a=Zz');
    fwrite($h, 'more');
    fclose($h);
    restore_error_handler();
    return count($seen);
});

/* dechunk. */
$scvShow('dechunk', fn() => [
    $scvRun("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", 'dechunk', []),
    $scvRun("4;ext=1\r\nWiki\r\n0\r\n\r\n", 'dechunk', []),
    $scvRun("A\r\n0123456789\r\n0\r\n\r\n", 'dechunk', []),
    $scvRun('not chunked at all', 'dechunk', []),
    $scvRun("4\r\nWiki\r\nZZZ", 'dechunk', []),
    $scvRun("4\r\nWi", 'dechunk', []),
]);
?>
--EXPECT--
registered names => array (\n  0 => 'convert.*',\n  1 => 'dechunk',\n)
unknown convert => false
encode => 'YWJjZGVmZ2hp'
explicit null params => array (\n  0 => false,\n  1 => 'stream_filter_append(): Unable to create or locate filter "convert.base64-encode"',\n)
decode => 'abcdefghi'
decode skips what is not in the alphabet => 'abc'
decode padding closes the group => array (\n  0 => 'a',\n  1 => 'ab',\n  2 => 'abcd',\n)
base64 line lengths round down to four => array (\n  0 => 'YWJjZGVmZ2hpamts',\n  1 => 'YWJjZGVmZ2hpamts',\n  2 => 'YWJj\nZGVm\nZ2hp\namts',\n  3 => 'YWJj\nZGVm\nZ2hp\namts',\n  4 => 'YWJjZGVm\nZ2hpamts',\n)
qp encode => 'h=C3=A9llo=3D=00world  =0D=0Atab	here'
qp encode binary => 'h=C3=A9llo=3D=00world=20=20=0D=0Atab=09here'
qp line lengths need four => array (\n  0 => 'abcdef=FFghi',\n  1 => 'abcdef=FFghi',\n  2 => 'abc=\ndef=\n=FF=\nghi',\n  3 => 'abcd=\nef=\n=FFg=\nhi',\n  4 => 'abcdef=\n=FFghi',\n)
qp lines only exist with a length => array (\n  0 => 'a=0D=0Ab',\n  1 => 'a\r\nb',\n)
qp whitespace sees only this read => array (\n  0 => 'x0 Ty',\n  1 => 'a b=20c',\n  2 => 'a=20\r\nb',\n  3 => 'x0=20Ty',\n)
qp decode => 'héllo=' . "\0" . 'world \r\n'
qp decode soft breaks => array (\n  0 => 'abcdef',\n  1 => 'abcdef',\n  2 => 'trail',\n)
chunk size does not change a round trip => array (\n  0 => true,\n  1 => true,\n  2 => true,\n  3 => true,\n  4 => true,\n)
removing an upstream filter => 'QUJD'
rewind re-opens the chain => array (\n  0 => 'YWI=',\n  1 => 'YWI=',\n)
a refusal is reported per write and never at close => 2
dechunk => array (\n  0 => 'Wikipedia',\n  1 => 'Wiki',\n  2 => '0123456789',\n  3 => 'not chunked at all',\n  4 => 'WikiZZZ',\n  5 => 'Wi',\n)
