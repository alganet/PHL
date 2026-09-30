--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zip: the read-only zip:// wrapper, and the stream getStream() hands out
--DESCRIPTION--
One device, two doors -- and php tells them apart in the metadata: a handle
opened through `zip://` names a WRAPPER and one getStream() built names none,
carrying the ENTRY as its uri instead. Neither is seekable: a member is
decompressed forwards.
--SKIPIF--
<?php
/* The answers pinned here are libzip 1.7's, which is what this engine derives;
 * a php linked against a newer libzip answers that version's. */
if (!class_exists('ZipArchive')) {
    die("skip this php has no ext/zip\n");
}
if (!str_starts_with(ZipArchive::LIBZIP_VERSION, '1.7.')) {
    die("skip php here links libzip " . ZipArchive::LIBZIP_VERSION . ", not 1.7\n");
}
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-zipw-' . getmypid();
@mkdir($dir);
/* Windows spells a path with backslashes and this test builds its own with
 * slashes, so every masking goes through one normalizer. */
function mask($s) {
    global $dir;
    $s = str_replace('\\', '/', (string) $s);
    return str_replace(str_replace('\\', '/', $dir), '<dir>', $s);
}
set_error_handler(function ($n, $s) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', mask($s), "\n";
    return true;
});
function show($label, $cb) {
    echo $label, ': ';
    try { echo var_export($cb(), true), "\n"; }
    catch (Throwable $e) {
        echo get_class($e), ': ', mask($e->getMessage()), "\n";
    }
}
function meta($h) {
    $m = stream_get_meta_data($h);
    unset($m['timed_out'], $m['blocked'], $m['unread_bytes']);
    if (isset($m['uri'])) { $m['uri'] = mask($m['uri']); }
    ksort($m);
    return $m;
}
$p = $dir . '/t.zip';
$z = new ZipArchive();
$z->open($p, ZipArchive::CREATE);
$z->addFromString('a.txt', 'plain bytes');
$z->addFromString('big.bin', str_repeat('WX', 700));
$z->addEmptyDir('d');
$z->close();

echo "-- the wrapper is registered and reads\n";
var_dump(in_array('zip', stream_get_wrappers(), true));
show('file_get_contents', fn() => file_get_contents('zip://' . $p . '#a.txt'));
show('deflated member', fn() => file_get_contents('zip://' . $p . '#big.bin') === str_repeat('WX', 700));
$h = fopen('zip://' . $p . '#big.bin', 'r');
print_r(meta($h));
show('read', fn() => strlen(stream_get_contents($h)));
show('eof', fn() => feof($h));
show('seek is refused', fn() => @fseek($h, 0));
fclose($h);

echo "-- and nothing else\n";
show('missing entry', fn() => @file_get_contents('zip://' . $p . '#nope'));
show('missing archive', fn() => @file_get_contents('zip://' . $dir . '/nope.zip#a'));
show('no fragment', fn() => @file_get_contents('zip://' . $p));
show('write mode', fn() => @fopen('zip://' . $p . '#a.txt', 'w'));
show('file_exists', fn() => file_exists('zip://' . $p . '#a.txt'));
show('stat', fn() => @stat('zip://' . $p . '#a.txt'));
show('opendir', fn() => @opendir('zip://' . $p));

echo "-- getStream names the entry and no wrapper\n";
$y = new ZipArchive();
$y->open($p);
$s = $y->getStream('a.txt');
print_r(meta($s));
show('contents', fn() => stream_get_contents($s));
show('tell', fn() => ftell($s));
fclose($s);
show('by index', function () use ($y) {
    $t = $y->getStreamIndex(1);
    $n = strlen(stream_get_contents($t));
    fclose($t);
    return $n;
});
show('a name that is not there', fn() => $y->getStreamName('nope'));
show('status', fn() => [$y->status, $y->getStatusString()]);
/* php's answer here is its library's read BUFFER showing through: a handle
 * something has already been read from outlives the close, and one nothing has
 * been read from dies with it. */
echo "a stream that has been read OUTLIVES the close:\n";
$u = $y->getStream('a.txt');
show('first bytes', fn() => fread($u, 5));
show('close', fn() => $y->close());
show('the rest', fn() => fread($u, 99));
fclose($u);
$w = new ZipArchive();
$w->open($p);
$v = $w->getStream('a.txt');
show('close before any read', fn() => $w->close());
show('and the read is refused', fn() => @fread($v, 5));
fclose($v);
$rm = function ($d) use (&$rm) {
    if (!is_dir($d)) { @unlink($d); return; }
    foreach (scandir($d) as $f) { if ($f !== '.' && $f !== '..') { $rm("$d/$f"); } }
    @rmdir($d);
};
$rm($dir);
--EXPECT--
-- the wrapper is registered and reads
bool(true)
file_get_contents: 'plain bytes'
deflated member: true
Array
(
    [eof] => 
    [mode] => r
    [seekable] => 
    [stream_type] => zip
    [uri] => zip://<dir>/t.zip#big.bin
    [wrapper_type] => zip wrapper
)
read: 1400
eof: true
seek is refused: -1
-- and nothing else
missing entry: false
missing archive: false
no fragment: false
write mode: false
file_exists: false
stat: false
opendir: false
-- getStream names the entry and no wrapper
Array
(
    [eof] => 
    [mode] => rb
    [seekable] => 
    [stream_type] => zip
    [uri] => a.txt
)
contents: 'plain bytes'
tell: 11
by index: 1400
a name that is not there: false
status: array (
  0 => 9,
  1 => 'No such file',
)
a stream that has been read OUTLIVES the close:
first bytes: 'plain'
close: true
the rest: ' bytes'
close before any read: true
and the read is refused: false
