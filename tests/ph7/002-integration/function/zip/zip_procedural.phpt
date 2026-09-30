--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zip: the ten deprecated procedural verbs, and the two resources they hand out
--DESCRIPTION--
php deprecated all ten in 8.0 and has kept them since, so the notice belongs to
every CALL. They are a one-way cursor over the entry table, and the pair of
handles behind them are resources rather than objects -- which is the only thing
`get_resource_type()` can tell them apart by.
--SKIPIF--
<?php
/* a capability guard: CI's Windows php is built without ext/zip */
if (!class_exists('ZipArchive')) {
    die("skip this php has no ext/zip\n");
}
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-zipp-' . getmypid();
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
$p = $dir . '/t.zip';
$z = new ZipArchive();
$z->open($p, ZipArchive::CREATE);
$z->addFromString('a.txt', str_repeat('a', 40));
$z->addFromString('b.bin', str_repeat('xy', 400));
$z->addEmptyDir('d');
$z->close();

echo "-- the cursor\n";
$r = zip_open($p);
show('what it is', fn() => [get_debug_type($r), get_resource_type($r), is_resource($r)]);
while ($e = zip_read($r)) {
    printf("  %-8s size=%-5d method=%s kind=%s\n", zip_entry_name($e),
        zip_entry_filesize($e), var_export(zip_entry_compressionmethod($e), true),
        get_resource_type($e));
    show('  open', fn() => zip_entry_open($r, $e));
    show('  read 10', fn() => zip_entry_read($e, 10));
    show('  read rest', fn() => strlen(zip_entry_read($e, 100000)));
    show('  close', fn() => zip_entry_close($e));
}
show('past the end', fn() => zip_read($r));
show('zip_close', fn() => zip_close($r));

echo "-- what it refuses\n";
show('missing', fn() => zip_open($dir . '/nope.zip'));
show('a directory', fn() => zip_open($dir));
show('empty name', fn() => zip_open(''));
file_put_contents($dir . '/plain.bin', 'not a zip at all, not even close');
show('not an archive', fn() => zip_open($dir . '/plain.bin'));
foreach ([
    'zip_read' => fn() => zip_read(9),
    'zip_close' => fn() => zip_close(9),
    'zip_entry_open' => fn() => zip_entry_open(9, 9),
    'zip_entry_close' => fn() => zip_entry_close(9),
    'zip_entry_read' => fn() => zip_entry_read(9),
    'zip_entry_name' => fn() => zip_entry_name(9),
    'zip_entry_filesize' => fn() => zip_entry_filesize(9),
    'zip_entry_compressedsize' => fn() => zip_entry_compressedsize(9),
    'zip_entry_compressionmethod' => fn() => zip_entry_compressionmethod(9),
] as $name => $cb) {
    show($name . ' on a non-resource', $cb);
}
$rm = function ($d) use (&$rm) {
    if (!is_dir($d)) { @unlink($d); return; }
    foreach (scandir($d) as $f) { if ($f !== '.' && $f !== '..') { $rm("$d/$f"); } }
    @rmdir($d);
};
$rm($dir);
--EXPECT--
-- the cursor
  [8192] Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead
what it is: array (
  0 => 'resource (Zip Directory)',
  1 => 'Zip Directory',
  2 => true,
)
  [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_name() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_filesize() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_compressionmethod() is deprecated since 8.0, use ZipArchive::statIndex() instead
  a.txt    size=40    method='deflated' kind=Zip Entry
  open:   [8192] Function zip_entry_open() is deprecated since 8.0
true
  read 10:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
'aaaaaaaaaa'
  read rest:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
30
  close:   [8192] Function zip_entry_close() is deprecated since 8.0
true
  [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_name() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_filesize() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_compressionmethod() is deprecated since 8.0, use ZipArchive::statIndex() instead
  b.bin    size=800   method='deflated' kind=Zip Entry
  open:   [8192] Function zip_entry_open() is deprecated since 8.0
true
  read 10:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
'xyxyxyxyxy'
  read rest:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
790
  close:   [8192] Function zip_entry_close() is deprecated since 8.0
true
  [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_name() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_filesize() is deprecated since 8.0, use ZipArchive::statIndex() instead
  [8192] Function zip_entry_compressionmethod() is deprecated since 8.0, use ZipArchive::statIndex() instead
  d/       size=0     method='stored' kind=Zip Entry
  open:   [8192] Function zip_entry_open() is deprecated since 8.0
true
  read 10:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
''
  read rest:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
0
  close:   [8192] Function zip_entry_close() is deprecated since 8.0
true
  [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
past the end:   [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
false
zip_close:   [8192] Function zip_close() is deprecated since 8.0, use ZipArchive::close() instead
NULL
-- what it refuses
missing:   [8192] Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead
9
a directory:   [8192] Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead
28
empty name:   [8192] Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead
ValueError: zip_open(): Argument #1 ($filename) must not be empty
not an archive:   [8192] Function zip_open() is deprecated since 8.0, use ZipArchive::open() instead
19
zip_read on a non-resource:   [8192] Function zip_read() is deprecated since 8.0, use ZipArchive::statIndex() instead
TypeError: zip_read(): Argument #1 ($zip) must be of type resource, int given
zip_close on a non-resource:   [8192] Function zip_close() is deprecated since 8.0, use ZipArchive::close() instead
TypeError: zip_close(): Argument #1 ($zip) must be of type resource, int given
zip_entry_open on a non-resource:   [8192] Function zip_entry_open() is deprecated since 8.0
TypeError: zip_entry_open(): Argument #1 ($zip_dp) must be of type resource, int given
zip_entry_close on a non-resource:   [8192] Function zip_entry_close() is deprecated since 8.0
TypeError: zip_entry_close(): Argument #1 ($zip_entry) must be of type resource, int given
zip_entry_read on a non-resource:   [8192] Function zip_entry_read() is deprecated since 8.0, use ZipArchive::getFromIndex() instead
TypeError: zip_entry_read(): Argument #1 ($zip_entry) must be of type resource, int given
zip_entry_name on a non-resource:   [8192] Function zip_entry_name() is deprecated since 8.0, use ZipArchive::statIndex() instead
TypeError: zip_entry_name(): Argument #1 ($zip_entry) must be of type resource, int given
zip_entry_filesize on a non-resource:   [8192] Function zip_entry_filesize() is deprecated since 8.0, use ZipArchive::statIndex() instead
TypeError: zip_entry_filesize(): Argument #1 ($zip_entry) must be of type resource, int given
zip_entry_compressedsize on a non-resource:   [8192] Function zip_entry_compressedsize() is deprecated since 8.0, use ZipArchive::statIndex() instead
TypeError: zip_entry_compressedsize(): Argument #1 ($zip_entry) must be of type resource, int given
zip_entry_compressionmethod on a non-resource:   [8192] Function zip_entry_compressionmethod() is deprecated since 8.0, use ZipArchive::statIndex() instead
TypeError: zip_entry_compressionmethod(): Argument #1 ($zip_entry) must be of type resource, int given
