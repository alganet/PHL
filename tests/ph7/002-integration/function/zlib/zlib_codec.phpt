--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zlib: three framings, the buffer loop and the incremental contexts
--FILE--
<?php
/* What ext/zlib ANSWERS. The compressed bytes themselves belong to whichever
 * libz this build linked, so nothing below pins one: what is pinned is the
 * FRAMING each door writes and reads, the round trips, and the diagnostics --
 * all of which are php's rather than the library's. */
error_reporting(E_ALL);
set_error_handler(function ($n, $s) { echo "  [$n] $s\n"; return true; });
function show($label, $cb) {
    echo $label, ': ';
    try {
        $r = $cb();
        echo is_string($r) ? var_export($r, true) : var_export($r, true), "\n";
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
$d = 'hello hello hello world';

echo "-- each door writes its own framing\n";
printf("gzencode   %s\n", bin2hex(substr(gzencode($d), 0, 3)));
printf("gzcompress %s\n", bin2hex(substr(gzcompress($d), 0, 1)));
printf("gzdeflate  %d\n", strlen(gzdeflate($d)) < strlen($d) ? 1 : 0);
printf("zlib_encode gzip %s\n", bin2hex(substr(zlib_encode($d, ZLIB_ENCODING_GZIP), 0, 3)));
/* The same three, reached through the OTHER doors' $encoding argument. */
var_dump(gzencode($d, -1, ZLIB_ENCODING_DEFLATE) === gzcompress($d),
    gzcompress($d, -1, ZLIB_ENCODING_RAW) === gzdeflate($d),
    gzdeflate($d, -1, ZLIB_ENCODING_GZIP) === gzencode($d));

echo "-- and each decoder takes only its own\n";
show('gzuncompress(zlib)', fn() => gzuncompress(gzcompress($d)));
show('gzinflate(raw)', fn() => gzinflate(gzdeflate($d)));
show('gzdecode(gzip)', fn() => gzdecode(gzencode($d)));
show('gzdecode(zlib)', fn() => gzdecode(gzcompress($d)));
show('gzinflate(gzip)', fn() => gzinflate(gzencode($d)));
show('gzuncompress(raw)', fn() => gzuncompress(gzdeflate($d)));
echo "-- zlib_decode is the one that works it out\n";
show('zlib_decode(gzip)', fn() => zlib_decode(gzencode($d)));
show('zlib_decode(zlib)', fn() => zlib_decode(gzcompress($d)));
show('zlib_decode(raw)', fn() => zlib_decode(gzdeflate($d)));

echo "-- every level, and the two ends of the range\n";
foreach ([-1, 0, 1, 6, 9] as $level) {
    printf("%2d %s\n", $level, gzuncompress(gzcompress($d, $level)));
}
show('level -2', fn() => gzcompress($d, -2));
show('level 10', fn() => gzcompress($d, 10));
show('encoding 7', fn() => gzencode($d, -1, 7));
show('zlib_encode encoding 7', fn() => zlib_encode($d, 7));

echo "-- the empty string is compressible and not decompressible\n";
var_dump(strlen(gzencode('')) > 0, strlen(gzcompress('')) > 0, strlen(gzdeflate('')) > 0);
show('gzinflate("")', fn() => gzinflate(''));
show('gzuncompress("")', fn() => gzuncompress(''));
show('gzdecode("")', fn() => gzdecode(''));
show('gzinflate(garbage)', fn() => gzinflate('not compressed at all'));
show('gzinflate(truncated)', fn() => gzinflate(substr(gzdeflate($d), 0, 5)));

echo "-- trailing bytes are ignored, a second gzip MEMBER is not read\n";
show('trailing junk', fn() => gzinflate(gzdeflate($d) . 'XXXX'));
show('two members', fn() => gzdecode(gzencode('a') . gzencode('b')));

echo "-- \$max_length stops the buffer GROWING, it does not truncate\n";
$c = gzcompress($d);            /* 23 bytes out */
foreach ([0, 1, 22, 23, 24, 100] as $max) {
    show("max $max", fn() => gzuncompress($c, $max));
}
$big = gzcompress(str_repeat('payload-', 10000));   /* 80000 bytes out */
show('big max 79999', fn() => strlen(gzuncompress($big, 79999)));
show('max -1', fn() => gzuncompress($c, -1));

echo "-- the incremental pair\n";
$ctx = deflate_init(ZLIB_ENCODING_GZIP);
$a = deflate_add($ctx, 'part one ', ZLIB_NO_FLUSH);
$b = deflate_add($ctx, 'part two', ZLIB_FINISH);
var_dump(gzdecode($a . $b));
$ctx = inflate_init(ZLIB_ENCODING_GZIP);
$gz = gzencode('streamed content here');
var_dump(inflate_add($ctx, substr($gz, 0, 10)),
    inflate_add($ctx, substr($gz, 10), ZLIB_FINISH),
    inflate_get_status($ctx), inflate_get_read_len($ctx));
$ctx = inflate_init(ZLIB_ENCODING_RAW);
var_dump(inflate_add($ctx, ''), inflate_get_status($ctx), inflate_get_read_len($ctx));
show('inflate_add(garbage)', function () {
    $c = inflate_init(ZLIB_ENCODING_RAW);
    return inflate_add($c, "\xff\xff\xff\xff");
});
echo "-- a finished context takes another member\n";
$ctx = inflate_init(ZLIB_ENCODING_RAW);
var_dump(inflate_add($ctx, gzdeflate('abc'), ZLIB_FINISH),
    inflate_add($ctx, gzdeflate('def'), ZLIB_FINISH), inflate_get_status($ctx));

echo "-- what the options screen refuses\n";
show('bad encoding', fn() => deflate_init(7));
show('bad level', fn() => deflate_init(ZLIB_ENCODING_RAW, ['level' => 99]));
show('bad memory', fn() => deflate_init(ZLIB_ENCODING_RAW, ['memory' => 0]));
show('bad window', fn() => deflate_init(ZLIB_ENCODING_RAW, ['window' => 7]));
show('bad window (inflate)', fn() => inflate_init(ZLIB_ENCODING_RAW, ['window' => 7]));
show('bad strategy', fn() => deflate_init(ZLIB_ENCODING_RAW, ['strategy' => 99]));
show('empty dictionary member', fn() => deflate_init(ZLIB_ENCODING_RAW, ['dictionary' => ['', 'x']]));
show('NUL in dictionary', fn() => deflate_init(ZLIB_ENCODING_RAW, ['dictionary' => ["a\0b"]]));
show('bad flush mode', function () {
    $c = deflate_init(ZLIB_ENCODING_RAW);
    return deflate_add($c, 'x', 99);
});
echo "-- an unknown option is ignored, and an OBJECT is read by its properties\n";
var_dump(get_debug_type(deflate_init(ZLIB_ENCODING_RAW, ['bogus' => 1])),
    get_debug_type(deflate_init(ZLIB_ENCODING_RAW, (object) ['level' => 9])));
show('bad level on an object', fn() => deflate_init(ZLIB_ENCODING_RAW, (object) ['level' => 99]));

echo "-- a dictionary the two ends agree on\n";
$dict = ['hello', 'world'];
$dc = deflate_init(ZLIB_ENCODING_DEFLATE, ['dictionary' => $dict]);
$out = deflate_add($dc, 'hello world hello', ZLIB_FINISH);
$ic = inflate_init(ZLIB_ENCODING_DEFLATE, ['dictionary' => $dict]);
var_dump(inflate_add($ic, $out, ZLIB_FINISH));
--EXPECT--
-- each door writes its own framing
gzencode   1f8b08
gzcompress 78
gzdeflate  1
zlib_encode gzip 1f8b08
bool(true)
bool(true)
bool(true)
-- and each decoder takes only its own
gzuncompress(zlib): 'hello hello hello world'
gzinflate(raw): 'hello hello hello world'
gzdecode(gzip): 'hello hello hello world'
gzdecode(zlib):   [2] gzdecode(): data error
false
gzinflate(gzip):   [2] gzinflate(): data error
false
gzuncompress(raw):   [2] gzuncompress(): data error
false
-- zlib_decode is the one that works it out
zlib_decode(gzip): 'hello hello hello world'
zlib_decode(zlib): 'hello hello hello world'
zlib_decode(raw): 'hello hello hello world'
-- every level, and the two ends of the range
-1 hello hello hello world
 0 hello hello hello world
 1 hello hello hello world
 6 hello hello hello world
 9 hello hello hello world
level -2: ValueError: gzcompress(): Argument #2 ($level) must be between -1 and 9
level 10: ValueError: gzcompress(): Argument #2 ($level) must be between -1 and 9
encoding 7: ValueError: gzencode(): Argument #3 ($encoding) must be one of ZLIB_ENCODING_RAW, ZLIB_ENCODING_GZIP, or ZLIB_ENCODING_DEFLATE
zlib_encode encoding 7: ValueError: zlib_encode(): Argument #2 ($encoding) must be one of ZLIB_ENCODING_RAW, ZLIB_ENCODING_GZIP, or ZLIB_ENCODING_DEFLATE
-- the empty string is compressible and not decompressible
bool(true)
bool(true)
bool(true)
gzinflate(""):   [2] gzinflate(): data error
false
gzuncompress(""):   [2] gzuncompress(): data error
false
gzdecode(""):   [2] gzdecode(): data error
false
gzinflate(garbage):   [2] gzinflate(): data error
false
gzinflate(truncated):   [2] gzinflate(): data error
false
-- trailing bytes are ignored, a second gzip MEMBER is not read
trailing junk: 'hello hello hello world'
two members: 'a'
-- $max_length stops the buffer GROWING, it does not truncate
max 0: 'hello hello hello world'
max 1:   [2] gzuncompress(): insufficient memory
false
max 22:   [2] gzuncompress(): insufficient memory
false
max 23: 'hello hello hello world'
max 24: 'hello hello hello world'
max 100: 'hello hello hello world'
big max 79999: 80000
max -1: ValueError: gzuncompress(): Argument #2 ($max_length) must be greater than or equal to 0
-- the incremental pair
string(17) "part one part two"
string(0) ""
string(21) "streamed content here"
int(1)
int(41)
string(0) ""
int(0)
int(0)
inflate_add(garbage):   [2] inflate_add(): data error
false
-- a finished context takes another member
string(3) "abc"
string(3) "def"
int(1)
-- what the options screen refuses
bad encoding: ValueError: deflate_init(): Argument #1 ($encoding) must be one of ZLIB_ENCODING_RAW, ZLIB_ENCODING_GZIP, or ZLIB_ENCODING_DEFLATE
bad level: ValueError: deflate_init(): "level" option must be between -1 and 9
bad memory: ValueError: deflate_init(): "memory" option must be between 1 and 9
bad window: ValueError: deflate_init(): "window" option must be between 8 and 15
bad window (inflate): ValueError: zlib window size (logarithm) (7) must be within 8..15
bad strategy: ValueError: deflate_init(): "strategy" option must be one of ZLIB_FILTERED, ZLIB_HUFFMAN_ONLY, ZLIB_RLE, ZLIB_FIXED, or ZLIB_DEFAULT_STRATEGY
empty dictionary member: ValueError: deflate_init(): Argument #2 ($options) must not contain empty strings
NUL in dictionary: ValueError: deflate_init(): Argument #2 ($options) must not contain strings with null bytes
bad flush mode: ValueError: deflate_add(): Argument #3 ($flush_mode) must be one of ZLIB_NO_FLUSH, ZLIB_PARTIAL_FLUSH, ZLIB_SYNC_FLUSH, ZLIB_FULL_FLUSH, ZLIB_BLOCK, or ZLIB_FINISH
-- an unknown option is ignored, and an OBJECT is read by its properties
string(14) "DeflateContext"
string(14) "DeflateContext"
bad level on an object: ValueError: deflate_init(): "level" option must be between -1 and 9
-- a dictionary the two ends agree on
string(17) "hello world hello"
