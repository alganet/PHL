--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/zip: build an archive, change it, read it back and extract it
--DESCRIPTION--
The whole of what an archive IS, measured THROUGH the archive rather than
against stored bytes: a compressed member's length belongs to whichever libz
built it, so nothing here prints one. What it does pin is libzip's TWO-FACED
entry table -- a delete keeps its index, FL_UNCHANGED still reads the original,
unchangeAll() puts it all back -- and the answers a fresh entry gives before it
has ever been written.
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
$dir = sys_get_temp_dir() . '/phlt-zip-' . getmypid();
@mkdir($dir);
/* every diagnostic through one handler, so no absolute path reaches the output */
set_error_handler(function ($n, $s) {
    if (!(error_reporting() & $n)) { return true; }
    echo '  [', $n, '] ', mask($s), "\n";
    return true;
});
/* Windows spells a path with backslashes and this test builds its own with
 * slashes, so every masking goes through one normalizer. libzip may name the
 * directory with symlinks resolved (macOS's /var is /private/var), so both
 * spellings mask. */
function mask($s) {
    global $dir;
    $s = str_replace('\\', '/', (string) $s);
    $real = realpath($dir) ?: $dir;
    return str_replace([str_replace('\\', '/', $real), str_replace('\\', '/', $dir)], '<dir>', $s);
}
function show($label, $cb) {
    echo $label, ': ';
    try { echo var_export($cb(), true), "\n"; }
    catch (Throwable $e) {
        echo get_class($e), ': ', mask($e->getMessage()), "\n";
    }
}
function entries(ZipArchive $z, $flags = 0) {
    for ($i = 0; $i < $z->numFiles; $i++) {
        $s = $z->statIndex($i, $flags);
        if ($s === false) { printf("  %d: (gone)\n", $i); continue; }
        printf("  %d: %-14s size=%-5d method=%d crc=%u enc=%d mtime=%d comment=%s\n",
            $i, $s['name'], $s['size'], $s['comp_method'], $s['crc'],
            $s['encryption_method'], $s['mtime'],
            var_export($z->getCommentIndex($i), true));
    }
}
$p = $dir . '/t.zip';
@unlink($p);

echo "-- create\n";
$z = new ZipArchive();
show('open', fn() => $z->open($p, ZipArchive::CREATE));
show('filename', fn() => mask($z->filename));
show('addFromString', fn() => $z->addFromString('hello.txt', 'Hello, World!'));
show('lastId', fn() => $z->lastId);
show('addEmptyDir', fn() => $z->addEmptyDir('sub'));
show('addFromString sub', fn() => $z->addFromString('sub/big.bin', str_repeat('AB', 600)));
show('addFromString again', fn() => $z->addFromString('sub/big.bin', str_repeat('AB', 600)));
show('addFromString no overwrite', fn() => $z->addFromString('hello.txt', 'x', 0));
show('numFiles', fn() => [$z->numFiles, count($z), $z->lastId]);
/* the stamps go on FIRST: an entry nobody set one on carries the clock, and a
 * test that printed that would pass once and never again */
foreach (['hello.txt' => 1234567890, 'sub/big.bin' => 1400000000, 'sub/' => 1000000000] as $n => $t) {
    show("setMtimeName $n", fn() => $z->setMtimeName($n, $t));
}
echo "an entry nobody has written yet describes its SOURCE:\n";
entries($z);
show('getFromName before close', fn() => $z->getFromName('hello.txt'));
show('status', fn() => [$z->status, $z->getStatusString()]);
show('setCommentName', fn() => $z->setCommentName('hello.txt', 'the greeting'));
show('setArchiveComment', fn() => $z->setArchiveComment('an archive comment'));
show('close', fn() => $z->close());
show('after close', fn() => [$z->numFiles, $z->status, $z->filename, $z->lastId]);

echo "-- read it back\n";
$y = new ZipArchive();
show('open', fn() => $y->open($p));
show('archive', fn() => [$y->numFiles, count($y), $y->comment, $y->getArchiveComment()]);
entries($y);
show('getFromName', fn() => $y->getFromName('hello.txt'));
show('getFromIndex len', fn() => $y->getFromIndex(0, 5));
show('getFromName big', fn() => strlen($y->getFromName('sub/big.bin')));
show('locateName', fn() => [$y->locateName('hello.txt'), $y->locateName('HELLO.TXT'),
    $y->locateName('HELLO.TXT', ZipArchive::FL_NOCASE), $y->locateName('big.bin', ZipArchive::FL_NODIR)]);
show('getNameIndex', fn() => [$y->getNameIndex(0), $y->getNameIndex(9)]);
show('statName miss', fn() => $y->statName('nope'));
show('status after miss', fn() => [$y->status, $y->getStatusString()]);
$o = $a = null;
show('externalAttributes', function () use ($y, &$o, &$a) {
    $ok = $y->getExternalAttributesIndex(0, $o, $a);
    return [$ok, $o, sprintf('0%o', $a >> 16)];
});
echo "-- change it without writing\n";
show('deleteName', fn() => $y->deleteName('hello.txt'));
show('the index stays', fn() => [$y->numFiles, $y->statName('hello.txt'), $y->getNameIndex(0)]);
show('renameName', fn() => $y->renameName('sub/big.bin', 'sub/renamed.bin'));
show('name now', fn() => $y->getNameIndex(2));
show('FL_UNCHANGED reads the original', fn() => $y->getNameIndex(2, ZipArchive::FL_UNCHANGED));
show('rename onto an existing name', fn() => $y->renameName('sub/renamed.bin', 'sub/'));
show('status', fn() => [$y->status, $y->getStatusString()]);
show('setArchiveComment', fn() => $y->setArchiveComment('changed'));
show('unchangeArchive', fn() => [$y->unchangeArchive(), $y->comment]);
show('unchangeAll', fn() => $y->unchangeAll());
echo "everything is back:\n";
entries($y);
show('close', fn() => $y->close());

echo "-- the file was not rewritten\n";
$md = md5_file($p);
$q = new ZipArchive();
show('open+close changes nothing', function () use ($q, $p, $md) {
    $q->open($p);
    $q->getFromName('hello.txt');
    return [$q->close(), $md === md5_file($p)];
});

echo "-- compression is a per-entry question\n";
$c = new ZipArchive();
$c->open($p);
show('setCompressionName store', fn() => $c->setCompressionName('sub/big.bin', ZipArchive::CM_STORE));
/* a method the BUILD cannot write is a build's answer, not php's, so the one
 * asked here is the one no build has: setCompression refuses it either way */
show('setCompressionIndex nonsense', fn() => $c->setCompressionIndex(0, 4242));
show('status', fn() => [$c->status, $c->getStatusString()]);
show('setCompressionName bad index', fn() => $c->setCompressionIndex(99, ZipArchive::CM_STORE));
show('clearError', fn() => [$c->clearError(), $c->status]);
show('close', fn() => $c->close());
$d = new ZipArchive();
$d->open($p);
entries($d);
show('bytes survived', fn() => $d->getFromName('sub/big.bin') === str_repeat('AB', 600));
$d->close();

echo "-- extract\n";
$out = $dir . '/out';
$e = new ZipArchive();
$e->open($p);
show('extractTo', fn() => $e->extractTo($out));
foreach (['hello.txt', 'sub', 'sub/big.bin'] as $n) {
    /* a DIRECTORY's stamp is not restored -- it is whatever the mkdir left,
     * which is the clock, so only a file's is printed */
    printf("  %-12s exists=%d dir=%d size=%s mtime=%s\n", $n, (int) file_exists("$out/$n"),
        (int) is_dir("$out/$n"),
        is_dir("$out/$n") ? '(dir)' : (string) @filesize("$out/$n"),
        is_dir("$out/$n") ? '(dir)' : (string) @filemtime("$out/$n"));
}
show('extractTo one', fn() => $e->extractTo($dir . '/one', ['hello.txt']));
show('extractTo missing', fn() => $e->extractTo($dir . '/two', ['nope']));
show('extractTo empty list', fn() => $e->extractTo($dir . '/two', []));
show('status', fn() => [$e->status, $e->getStatusString()]);
show('close', fn() => $e->close());

echo "-- a name cannot escape the destination\n";
$t = $dir . '/tr.zip';
@unlink($t);
$w = new ZipArchive();
$w->open($t, ZipArchive::CREATE);
foreach (['../escaped.txt', 'ok/../flat.txt', '/rooted.txt', 'a/b/c/deep.txt'] as $n) {
    $w->addFromString($n, $n);
    $w->setMtimeName($n, 1234567890);
}
$w->close();
$v = new ZipArchive();
$v->open($t);
entries($v);
show('extractTo', fn() => $v->extractTo($dir . '/tro'));
$found = [];
$it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir . '/tro',
    FilesystemIterator::SKIP_DOTS));
foreach ($it as $f) { $found[] = str_replace(mask($dir) . '/tro/', '', mask($f->getPathname())); }
sort($found);
print_r($found);
show('nothing above it', fn() => [file_exists($dir . '/escaped.txt'), file_exists('/rooted.txt')]);
$v->close();

echo "-- an archive with nothing left in it is REMOVED\n";
$k = new ZipArchive();
$k->open($t);
for ($i = $k->numFiles - 1; $i >= 0; $i--) { $k->deleteIndex($i); }
show('close', fn() => $k->close());
show('gone', fn() => file_exists($t));
$k2 = new ZipArchive();
show('a CREATE nobody added to', function () use ($k2, $dir) {
    $n = $dir . '/never.zip';
    return [$k2->open($n, ZipArchive::CREATE), $k2->close(), file_exists($n)];
});

echo "-- a NAME is a C string on the way IN and screened on the way OUT\n";
$n = $dir . '/n.zip';
@unlink($n);
$nz = new ZipArchive();
$nz->open($n, ZipArchive::CREATE);
show('addFromString', fn() => $nz->addFromString("x\0y", 'V'));
show('addEmptyDir', fn() => $nz->addEmptyDir("p\0q"));
$nz->close();
$nr = new ZipArchive();
$nr->open($n);
show('what was stored', function () use ($nr) {
    $r = [];
    for ($i = 0; $i < $nr->numFiles; $i++) { $r[] = $nr->getNameIndex($i); }
    return $r;
});
foreach ([
    'locateName' => fn() => $nr->locateName("x\0y"),
    'statName' => fn() => $nr->statName("x\0y"),
    'getFromName' => fn() => $nr->getFromName("x\0y"),
    'extractTo' => fn() => $nr->extractTo("x\0y"),
    'getStream' => fn() => $nr->getStream("x\0y"),
] as $verb => $cb) { show('  ' . $verb . ' screens it', $cb); }
foreach ([
    'setCommentName' => fn() => $nr->setCommentName("x\0y", 'c'),
    'getCommentName' => fn() => $nr->getCommentName("x\0y"),
    'setMtimeName' => fn() => $nr->setMtimeName("x\0y", 1234567890),
    'unchangeName' => fn() => $nr->unchangeName("x\0y"),
    'deleteName' => fn() => $nr->deleteName("x\0y"),
] as $verb => $cb) { show('  ' . $verb . ' matches the truncated one', $cb); }
show('the archive comment refuses one outright', fn() => $nr->setArchiveComment("c\0d"));
$nr->close();

echo "-- FL_UNCHANGED asked about an entry with no original\n";
$fz = new ZipArchive();
$fz->open($p);
$fz->addFromString('brand-new', 'F');
$fi = $fz->locateName('brand-new');
show('the changed name', fn() => $fz->getNameIndex($fi));
show('the original name', fn() => $fz->getNameIndex($fi, ZipArchive::FL_UNCHANGED));
show('the original description', fn() => $fz->statIndex($fi, ZipArchive::FL_UNCHANGED));
$fz->unchangeAll();
$fz->close();

echo "-- addGlob's flags are glob()'s, not the FL_ ones\n";
$gd = $dir . '/g';
@mkdir($gd);
file_put_contents($gd . '/one.txt', '1');
file_put_contents($gd . '/two.log', '2');
foreach ([0, GLOB_MARK, GLOB_NOSORT, GLOB_BRACE, ZipArchive::FL_OVERWRITE, ZipArchive::FL_NOCASE] as $fl) {
    $gz = new ZipArchive();
    $gz->open($dir . '/g.zip', ZipArchive::CREATE | ZipArchive::OVERWRITE);
    show('  flags ' . $fl, function () use ($gz, $gd, $fl) {
        $r = $gz->addGlob($gd . '/*.txt', $fl, ['remove_all_path' => true]);
        return $r === false ? false : array_map('mask', $r);
    });
    $gz->close();
}
$gb = new ZipArchive();
$gb->open($dir . '/gb.zip', ZipArchive::CREATE | ZipArchive::OVERWRITE);
show('GLOB_BRACE expands', fn() => $gb->addGlob($gd . '/*.{txt,log}', GLOB_BRACE,
    ['remove_all_path' => true]) !== false);
show('what it added', function () use ($gb) {
    $r = [];
    for ($i = 0; $i < $gb->numFiles; $i++) { $r[] = $gb->getNameIndex($i); }
    return $r;
});
show('a pattern that matches nothing', function () use ($gb, $dir) {
    $r = $gb->addGlob($dir . '/nosuch/*.txt');
    return $r === false ? false : array_map('mask', $r);
});
$gb->close();

echo "-- one damaged byte at a time, and what still reads\n";
/* A structural sweep rather than a case list: it is what turned up that php
 * does NOT check a local header's signature and that its inflate stops one
 * byte earlier than a one-shot finish does on a broken stream. */
$src = $dir . '/f.zip';
@unlink($src);
$fz = new ZipArchive();
$fz->open($src, ZipArchive::CREATE);
$fz->addFromString('a.txt', str_repeat('A', 300));
$fz->addEmptyDir('d');
$fz->addFromString('d/b.bin', str_repeat("\x01\x02\x03", 40));
$fz->setCommentName('a.txt', 'hello');
$fz->setArchiveComment('archive');
$fz->close();
$bytes = file_get_contents($src);
$hit = $dir . '/h.zip';
$rows = [];
for ($i = 0; $i < strlen($bytes); $i += 7) {
    foreach ([0x00, 0xFF, 0x41] as $b) {
        $bad = $bytes;
        $bad[$i] = chr($b);
        file_put_contents($hit, $bad);
        $hz = new ZipArchive();
        $r = @$hz->open($hit);
        $cell = is_int($r) ? 'E' . $r : 'ok';
        if ($r === true) {
            $read = 0;
            for ($k = 0; $k < $hz->numFiles; $k++) {
                $st = @$hz->statIndex($k);
                $read += $st === false ? 0 : (int) $st['size'];
                $v = @$hz->getFromIndex($k);
                $read += $v === false ? 0 : strlen($v);
            }
            /* 0xFF at 112 lands inside b.bin's deflate stream, and how much of
             * a broken stream inflates before zlib gives up is zlib's version's
             * answer (720 or 728 bytes read), not php's. */
            $cell .= ':' . (($i === 112 && $b === 0xFF) ? 'zlib' : $read);
            @$hz->close();
        }
        $rows[] = $i . '/' . sprintf('%02x', $b) . '=' . $cell;
    }
}
echo implode(' ', $rows), "\n";
echo 'rows=', count($rows), "\n";

echo "-- opening what is not an archive\n";
file_put_contents($dir . '/plain.bin', 'not a zip at all, not even close');
file_put_contents($dir . '/empty.bin', '');
$m = new ZipArchive();
foreach (['plain.bin', 'empty.bin', 'nothere.zip', ''] as $n) {
    show("open $n", fn() => $m->open($n === '' ? $dir : $dir . '/' . $n));
}
show('open EXCL on an existing file', fn() => $m->open($dir . '/plain.bin',
    ZipArchive::CREATE | ZipArchive::EXCL));
show('open RDONLY refuses a write', function () use ($m, $dir) {
    $r = $m->open($dir . '/plain.bin', ZipArchive::RDONLY);
    return $r;
});

/* tidy up */
$rm = function ($d) use (&$rm) {
    if (!is_dir($d)) { @unlink($d); return; }
    foreach (scandir($d) as $f) { if ($f !== '.' && $f !== '..') { $rm("$d/$f"); } }
    @rmdir($d);
};
$rm($dir);
--EXPECT--
-- create
open: true
filename: '<dir>/t.zip'
addFromString: true
lastId: 0
addEmptyDir: true
addFromString sub: true
addFromString again: true
addFromString no overwrite: false
numFiles: array (
  0 => 3,
  1 => 3,
  2 => -1,
)
setMtimeName hello.txt: true
setMtimeName sub/big.bin: true
setMtimeName sub/: true
an entry nobody has written yet describes its SOURCE:
  0: hello.txt      size=13    method=0 crc=0 enc=0 mtime=1234567890 comment=''
  1: sub/           size=0     method=0 crc=0 enc=0 mtime=1000000000 comment=''
  2: sub/big.bin    size=1200  method=0 crc=0 enc=0 mtime=1400000000 comment=''
getFromName before close: false
status: array (
  0 => 15,
  1 => 'Entry has been changed',
)
setCommentName: true
setArchiveComment: true
close: true
after close: array (
  0 => 0,
  1 => 0,
  2 => '',
  3 => -1,
)
-- read it back
open: true
archive: array (
  0 => 3,
  1 => 3,
  2 => 'an archive comment',
  3 => 'an archive comment',
)
  0: hello.txt      size=13    method=0 crc=3964322768 enc=0 mtime=1234567890 comment='the greeting'
  1: sub/           size=0     method=0 crc=0 enc=0 mtime=1000000000 comment=''
  2: sub/big.bin    size=1200  method=8 crc=784825680 enc=0 mtime=1400000000 comment=''
getFromName: 'Hello, World!'
getFromIndex len: 'Hello'
getFromName big: 1200
locateName: array (
  0 => 0,
  1 => false,
  2 => 0,
  3 => 2,
)
getNameIndex: array (
  0 => 'hello.txt',
  1 => false,
)
statName miss: false
status after miss: array (
  0 => 9,
  1 => 'No such file',
)
externalAttributes: array (
  0 => true,
  1 => 3,
  2 => '0100666',
)
-- change it without writing
deleteName: true
the index stays: array (
  0 => 3,
  1 => false,
  2 => false,
)
renameName: true
name now: 'sub/renamed.bin'
FL_UNCHANGED reads the original: 'sub/big.bin'
rename onto an existing name: false
status: array (
  0 => 18,
  1 => 'Invalid argument',
)
setArchiveComment: true
unchangeArchive: array (
  0 => true,
  1 => 'an archive comment',
)
unchangeAll: true
everything is back:
  0: hello.txt      size=13    method=0 crc=3964322768 enc=0 mtime=1234567890 comment='the greeting'
  1: sub/           size=0     method=0 crc=0 enc=0 mtime=1000000000 comment=''
  2: sub/big.bin    size=1200  method=8 crc=784825680 enc=0 mtime=1400000000 comment=''
close: true
-- the file was not rewritten
open+close changes nothing: array (
  0 => true,
  1 => true,
)
-- compression is a per-entry question
setCompressionName store: true
setCompressionIndex nonsense: false
status: array (
  0 => 16,
  1 => 'Compression method not supported',
)
setCompressionName bad index: false
clearError: array (
  0 => NULL,
  1 => 0,
)
close: true
  0: hello.txt      size=13    method=0 crc=3964322768 enc=0 mtime=1234567890 comment='the greeting'
  1: sub/           size=0     method=0 crc=0 enc=0 mtime=1000000000 comment=''
  2: sub/big.bin    size=1200  method=0 crc=784825680 enc=0 mtime=1400000000 comment=''
bytes survived: true
-- extract
extractTo: true
  hello.txt    exists=1 dir=0 size=13 mtime=1234567890
  sub          exists=1 dir=1 size=(dir) mtime=(dir)
  sub/big.bin  exists=1 dir=0 size=1200 mtime=1400000000
extractTo one: true
extractTo missing: false
extractTo empty list: false
status: array (
  0 => 9,
  1 => 'No such file',
)
close: true
-- a name cannot escape the destination
  0: ../escaped.txt size=14    method=0 crc=3394473955 enc=0 mtime=1234567890 comment=''
  1: ok/../flat.txt size=14    method=0 crc=1722348203 enc=0 mtime=1234567890 comment=''
  2: /rooted.txt    size=11    method=0 crc=2306442812 enc=0 mtime=1234567890 comment=''
  3: a/b/c/deep.txt size=14    method=0 crc=1840062368 enc=0 mtime=1234567890 comment=''
extractTo: true
Array
(
    [0] => a/b/c/deep.txt
    [1] => escaped.txt
    [2] => flat.txt
    [3] => rooted.txt
)
nothing above it: array (
  0 => false,
  1 => false,
)
-- an archive with nothing left in it is REMOVED
close: true
gone: false
a CREATE nobody added to: array (
  0 => true,
  1 => true,
  2 => false,
)
-- a NAME is a C string on the way IN and screened on the way OUT
addFromString: true
addEmptyDir: true
what was stored: array (
  0 => 'x',
  1 => 'p/',
)
  locateName screens it: ValueError: ZipArchive::locateName(): Argument #1 ($name) must not contain any null bytes
  statName screens it: ValueError: ZipArchive::statName(): Argument #1 ($name) must not contain any null bytes
  getFromName screens it: ValueError: ZipArchive::getFromName(): Argument #1 ($name) must not contain any null bytes
  extractTo screens it: ValueError: ZipArchive::extractTo(): Argument #1 ($pathto) must not contain any null bytes
  getStream screens it: ValueError: ZipArchive::getStream(): Argument #1 ($name) must not contain any null bytes
  setCommentName matches the truncated one: true
  getCommentName matches the truncated one: 'c'
  setMtimeName matches the truncated one: true
  unchangeName matches the truncated one: true
  deleteName matches the truncated one: true
the archive comment refuses one outright: false
-- FL_UNCHANGED asked about an entry with no original
the changed name: 'brand-new'
the original name: false
the original description: false
-- addGlob's flags are glob()'s, not the FL_ ones
  flags 0: array (
  0 => '<dir>/g/one.txt',
)
  flags 8: array (
  0 => '<dir>/g/one.txt',
)
  flags 32: array (
  0 => '<dir>/g/one.txt',
)
  flags 128: array (
  0 => '<dir>/g/one.txt',
)
  flags 8192:   [2] ZipArchive::addGlob(): At least one of the passed flags is invalid or not supported on this platform
false
  flags 1:   [2] ZipArchive::addGlob(): At least one of the passed flags is invalid or not supported on this platform
false
GLOB_BRACE expands: true
what it added: array (
  0 => 'one.txt',
  1 => 'two.log',
)
a pattern that matches nothing: array (
)
-- one damaged byte at a time, and what still reads
0/00=ok:840 0/ff=ok:840 0/41=ok:840 7/00=ok:840 7/ff=ok:840 7/41=ok:840 14/00=ok:840 14/ff=ok:840 14/41=ok:840 21/00=ok:840 21/ff=ok:840 21/41=ok:840 28/00=ok:840 28/ff=ok:540 28/41=ok:540 35/00=ok:540 35/ff=ok:540 35/41=ok:540 42/00=ok:840 42/ff=ok:840 42/41=ok:840 49/00=ok:840 49/ff=ok:840 49/41=ok:840 56/00=ok:840 56/ff=ok:840 56/41=ok:840 63/00=ok:840 63/ff=ok:840 63/41=ok:840 70/00=ok:840 70/ff=ok:840 70/41=ok:840 77/00=ok:840 77/ff=ok:840 77/41=ok:840 84/00=ok:840 84/ff=ok:840 84/41=ok:840 91/00=ok:840 91/ff=ok:840 91/41=ok:840 98/00=ok:840 98/ff=ok:840 98/41=ok:840 105/00=ok:840 105/ff=ok:840 105/41=ok:840 112/00=ok:720 112/ff=ok:zlib 112/41=ok:720 119/00=E19 119/ff=E19 119/41=E19 126/00=ok:840 126/ff=ok:840 126/41=ok:840 133/00=ok:840 133/ff=ok:840 133/41=ok:840 140/00=ok:840 140/ff=ok:840 140/41=ok:840 147/00=E19 147/ff=E21 147/41=E19 154/00=ok:840 154/ff=ok:840 154/41=ok:840 161/00=ok:840 161/ff=ok:540 161/41=ok:540 168/00=ok:840 168/ff=ok:840 168/41=ok:840 175/00=E19 175/ff=E19 175/41=E19 182/00=ok:840 182/ff=ok:840 182/41=ok:840 189/00=ok:840 189/ff=ok:840 189/41=ok:840 196/00=ok:840 196/ff=ok:840 196/41=ok:840 203/00=E19 203/ff=E21 203/41=E21 210/00=ok:840 210/ff=ok:840 210/41=ok:840 217/00=ok:840 217/ff=ok:840 217/41=ok:840 224/00=E19 224/ff=E19 224/41=E19 231/00=ok:840 231/ff=ok:720 231/41=ok:720 238/00=ok:840 238/ff=ok:840 238/41=ok:840 245/00=ok:840 245/ff=ok:840 245/41=ok:840 252/00=ok:840 252/ff=E21 252/41=E21 259/00=ok:840 259/ff=ok:840 259/41=ok:840 266/00=ok:840 266/ff=ok:720 266/41=ok:720 273/00=ok:840 273/ff=ok:840 273/41=ok:840 280/00=ok:840 280/ff=E1 280/41=E1 287/00=ok:840 287/ff=E19 287/41=E19 294/00=ok:840 294/ff=E21 294/41=E21 301/00=ok:840 301/ff=ok:840 301/41=ok:840
rows=132
-- opening what is not an archive
open plain.bin: 19
open empty.bin:   [8192] ZipArchive::open(): Using empty file as ZipArchive is deprecated
true
open nothere.zip: 9
open : 28
open EXCL on an existing file: 10
open RDONLY refuses a write: 19
