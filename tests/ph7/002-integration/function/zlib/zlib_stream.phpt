--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zlib: gzopen, compress.zlib:// and what a wrapper refuses
--FILE--
<?php
/* The ZLIB device: one set of ops behind two doors, and libz's own reading
 * rules over it -- transparent for a file that is not gzip framed, one stream
 * for a CONCATENATION of members, and no SEEK_END because the uncompressed
 * length is not knowable without decoding. */
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-zlib-' . getmypid();
/* Every diagnostic below names a path with this run's pid in it, and the two
 * engines never share one: the directory is masked out of everything printed. */
set_error_handler(function ($n, $s) use (&$dir) {
    echo '  [', $n, '] ', str_replace($dir, '<dir>', $s), "\n";
    return true;
});
@mkdir($dir);
$gz = $dir . '/f.gz';
file_put_contents($gz, gzencode(str_repeat("abc\n", 5)));
$plain = $dir . '/plain.txt';
file_put_contents($plain, "line one\nline two\n");

echo "-- gzopen reads it, and reads a PLAIN file just as happily\n";
$h = gzopen($gz, 'r');
var_dump(get_resource_type($h), gzread($h, 100));
var_dump(gzeof($h), gzread($h, 4), gzeof($h));
gzclose($h);
$h = gzopen($plain, 'rb');
var_dump(gzread($h, 100));
gzclose($h);

echo "-- the line readers\n";
$h = gzopen($gz, 'r');
var_dump(gzgets($h), gzgets($h, 3), gzgetc($h), gzgets($h, 1));
gzclose($h);
var_dump(gzfile($gz), gzfile($plain));
ob_start();
$n = readgzfile($gz);
$out = ob_get_clean();
var_dump($n, $out);
$h = gzopen($gz, 'r');
ob_start();
$n = gzpassthru($h);
var_dump($n, ob_get_clean());
gzclose($h);

echo "-- where it is, and where it can go\n";
$h = gzopen($gz, 'r');
gzread($h, 4);
var_dump(gztell($h), gzseek($h, 2), gztell($h), gzread($h, 2));
var_dump(gzseek($h, -1, SEEK_CUR), gztell($h), gzrewind($h), gztell($h));
var_dump(gzseek($h, -4, SEEK_END));
var_dump(gzseek($h, 1000), gztell($h), gzread($h, 4), gzeof($h));
gzclose($h);

echo "-- the stream functions are the SAME functions\n";
$h = gzopen($gz, 'r');
var_dump(fread($h, 6), ftell($h), feof($h), stream_get_line($h, 100, "\n"));
$meta = stream_get_meta_data($h);
var_dump($meta['stream_type'], $meta['mode'], $meta['seekable'],
    array_key_exists('wrapper_type', $meta), array_key_exists('uri', $meta), fstat($h));
fclose($h);
$h = fopen($plain, 'r');
var_dump(gzread($h, 5), gzclose($h));

echo "-- writing, and what a mode carries\n";
$w = $dir . '/w.gz';
$h = gzopen($w, 'w');
var_dump(gzwrite($h, "written data\n"), gzclose($h));
var_dump(bin2hex(substr(file_get_contents($w), 0, 3)), gzdecode(file_get_contents($w)),
    implode('', gzfile($w)));
$h = gzopen($dir . '/w9.gz', 'wb9f');
gzwrite($h, str_repeat('ab', 30));
gzclose($h);
var_dump(gzdecode(file_get_contents($dir . '/w9.gz')));
$h = gzopen($w, 'r');
var_dump(gzwrite($h, 'x'));
gzclose($h);
$h = gzopen($dir . '/w2.gz', 'w');
var_dump(gzread($h, 4));
gzclose($h);
var_dump(gzopen($gz, 'r+'), gzopen($gz, 'q'), gzopen($gz, 'x'),
    gzopen($dir . '/nope.gz', 'r'), gzfile($dir . '/nope.gz'), readgzfile($dir . '/nope.gz'));

echo "-- appending writes a second MEMBER, and the reader reads both\n";
$ap = $dir . '/ap.gz';
$h = gzopen($ap, 'a'); gzwrite($h, "one\n"); gzclose($h);
$h = gzopen($ap, 'a'); gzwrite($h, "two\n"); gzclose($h);
var_dump(file_get_contents('compress.zlib://' . $ap), implode('', gzfile($ap)),
    gzdecode(file_get_contents($ap)));

echo "-- the wrapper is the same device with a wrapper's metadata\n";
var_dump(file_get_contents('compress.zlib://' . $gz), file('compress.zlib://' . $gz));
$h = fopen('compress.zlib://' . $gz, 'r');
$meta = stream_get_meta_data($h);
var_dump($meta['wrapper_type'], $meta['stream_type'], str_replace($dir, '<dir>', $meta['uri']));
fclose($h);
var_dump(file_put_contents('compress.zlib://' . $dir . '/fp.gz', 'payload'),
    gzdecode(file_get_contents($dir . '/fp.gz')));
/* A zlib-framed or raw-deflate file is NOT gzip, so it comes back as it is. */
file_put_contents($dir . '/zl.z', gzcompress('zlib framed'));
var_dump(file_get_contents('compress.zlib://' . $dir . '/zl.z') === gzcompress('zlib framed'));
var_dump(include('compress.zlib://' . $dir . '/inc.php.gz'
    . (file_put_contents($dir . '/inc.php.gz', gzencode('<?php return 42;')) ? '' : '')));

echo "-- and what it does NOT do, which is what php's wrapper layer answers\n";
var_dump(file_exists('compress.zlib://' . $gz), @filesize('compress.zlib://' . $gz),
    unlink('compress.zlib://' . $dir . '/gone.gz'),
    rename('compress.zlib://' . $gz, 'compress.zlib://' . $gz . '2'),
    mkdir('compress.zlib://' . $dir . '/d'), rmdir('compress.zlib://' . $dir . '/d'),
    chmod('compress.zlib://' . $gz, 0644), opendir('compress.zlib://' . $dir));
echo "-- the same refusals belong to every built-in wrapper\n";
var_dump(unlink('php://memory'), unlink('data://text/plain,x'), unlink('glob:///tmp/*'),
    rename('php://memory', 'php://temp'), chmod('data://text/plain,x', 0644),
    /* chown/chgrp warn only where the platform has owners (Windows: silent) */
    (static function () { set_error_handler(static fn() => true);
        try { return chown('php://memory', 'root'); } finally { restore_error_handler(); } })(),
    (static function () { set_error_handler(static fn() => true);
        try { return chgrp('php://memory', 'root'); } finally { restore_error_handler(); } })());

echo "-- the zlib.* filters\n";
/* not [0]: Windows php registers convert.iconv.* ahead of it */
var_dump(in_array('zlib.*', stream_get_filters(), true));
$f = $dir . '/filtered.dat';
$h = fopen($f, 'w');
stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE);
fwrite($h, 'filtered ');
fwrite($h, 'payload');
fclose($h);
var_dump(gzinflate(file_get_contents($f)));
$h = fopen($f, 'r');
stream_filter_append($h, 'zlib.inflate', STREAM_FILTER_READ);
var_dump(stream_get_contents($h));
fclose($h);
/* The window parameter picks the framing on both sides. */
$h = fopen($dir . '/gzfilter.gz', 'w');
stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['window' => 31, 'level' => 9]);
fwrite($h, 'through the filter');
fclose($h);
var_dump(bin2hex(substr(file_get_contents($dir . '/gzfilter.gz'), 0, 3)),
    gzdecode(file_get_contents($dir . '/gzfilter.gz')));
$h = fopen($dir . '/zlfilter.z', 'w');
stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['window' => 15]);
fwrite($h, 'zlib framing');
fclose($h);
var_dump(gzuncompress(file_get_contents($dir . '/zlfilter.z')));
echo "-- what the filter refuses, and what it merely warns about\n";
$h = fopen('php://memory', 'w');
var_dump((bool) stream_filter_append($h, 'zlib.bogus'),
    (bool) stream_filter_append($h, 'zlib.deflate.foo'),
    (bool) stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['window' => 7]),
    (bool) stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['window' => 100]),
    (bool) stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['level' => 99]),
    (bool) stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, ['memory' => 0]),
    (bool) stream_filter_append($h, 'zlib.deflate', STREAM_FILTER_WRITE, null),
    (bool) stream_filter_append($h, 'zlib.inflate', STREAM_FILTER_READ, null),
    (bool) stream_filter_append($h, 'zlib.inflate', STREAM_FILTER_READ, ['level' => 99]));
fclose($h);
echo "-- a truncated member ends the stream where the bytes end\n";
file_put_contents($dir . '/trunc.gz', substr(gzencode(str_repeat('q', 400)), 0, 20));
var_dump(strlen(@file_get_contents('compress.zlib://' . $dir . '/trunc.gz')) > 0);

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- gzopen reads it, and reads a PLAIN file just as happily
string(6) "stream"
string(20) "abc
abc
abc
abc
abc
"
bool(true)
string(0) ""
bool(true)
string(18) "line one
line two
"
-- the line readers
string(4) "abc
"
string(2) "ab"
string(1) "c"
bool(false)
array(5) {
  [0]=>
  string(4) "abc
"
  [1]=>
  string(4) "abc
"
  [2]=>
  string(4) "abc
"
  [3]=>
  string(4) "abc
"
  [4]=>
  string(4) "abc
"
}
array(2) {
  [0]=>
  string(9) "line one
"
  [1]=>
  string(9) "line two
"
}
int(20)
string(20) "abc
abc
abc
abc
abc
"
int(20)
string(20) "abc
abc
abc
abc
abc
"
-- where it is, and where it can go
int(4)
int(0)
int(2)
string(2) "c
"
int(0)
int(3)
bool(true)
int(0)
  [2] gzseek(): SEEK_END is not supported
int(-1)
int(0)
int(1000)
string(0) ""
bool(true)
-- the stream functions are the SAME functions
string(6) "abc
ab"
int(6)
bool(false)
string(1) "c"
string(4) "ZLIB"
string(1) "r"
bool(true)
bool(false)
bool(false)
bool(false)
string(5) "line "
bool(true)
-- writing, and what a mode carries
int(13)
bool(true)
string(6) "1f8b08"
string(13) "written data
"
string(13) "written data
"
string(60) "abababababababababababababababababababababababababababababab"
int(0)
bool(false)
  [2] gzopen(): Cannot open a zlib stream for reading and writing at the same time!
  [2] gzopen(<dir>/f.gz): Failed to open stream: `q' is not a valid mode for fopen
  [2] gzopen(<dir>/f.gz): Failed to open stream: File exists
  [2] gzopen(<dir>/nope.gz): Failed to open stream: No such file or directory
  [2] gzfile(<dir>/nope.gz): Failed to open stream: No such file or directory
  [2] readgzfile(<dir>/nope.gz): Failed to open stream: No such file or directory
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
-- appending writes a second MEMBER, and the reader reads both
string(8) "one
two
"
string(8) "one
two
"
string(4) "one
"
-- the wrapper is the same device with a wrapper's metadata
string(20) "abc
abc
abc
abc
abc
"
array(5) {
  [0]=>
  string(4) "abc
"
  [1]=>
  string(4) "abc
"
  [2]=>
  string(4) "abc
"
  [3]=>
  string(4) "abc
"
  [4]=>
  string(4) "abc
"
}
string(4) "ZLIB"
string(4) "ZLIB"
string(26) "compress.zlib://<dir>/f.gz"
int(7)
string(7) "payload"
bool(true)
int(42)
-- and what it does NOT do, which is what php's wrapper layer answers
  [2] filesize(): stat failed for compress.zlib://<dir>/f.gz
  [2] unlink(): ZLIB does not allow unlinking
  [2] rename(): ZLIB wrapper does not support renaming
  [2] chmod(): Cannot call chmod() for a non-standard stream
  [2] opendir(compress.zlib://<dir>): Failed to open directory: not implemented
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
-- the same refusals belong to every built-in wrapper
  [2] unlink(): PHP does not allow unlinking
  [2] unlink(): RFC2397 does not allow unlinking
  [2] unlink(): glob does not allow unlinking
  [2] rename(): PHP wrapper does not support renaming
  [2] chmod(): Cannot call chmod() for a non-standard stream
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
-- the zlib.* filters
bool(true)
string(16) "filtered payload"
string(16) "filtered payload"
string(6) "1f8b08"
string(18) "through the filter"
string(12) "zlib framing"
-- what the filter refuses, and what it merely warns about
  [2] stream_filter_append(): Unable to create or locate filter "zlib.bogus"
  [2] stream_filter_append(): Unable to create or locate filter "zlib.deflate.foo"
  [2] stream_filter_append(): Unable to create or locate filter "zlib.deflate"
  [2] stream_filter_append(): Invalid parameter given for window size (100)
  [2] stream_filter_append(): Invalid compression level specified. (99)
  [2] stream_filter_append(): Invalid parameter given for memory level (0)
  [2] stream_filter_append(): Invalid filter parameter, ignored
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
-- a truncated member ends the stream where the bytes end
bool(true)
