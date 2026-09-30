--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/phar: build an archive, read it back, and RUN it
--DESCRIPTION--
The whole of what a .phar is: a stub that stops at __halt_compiler(), a manifest,
the entries' bytes and a signature over all of it. Everything here is measured
through the archive rather than against a stored file, because the compressed
and signed bytes belong to whichever libz and toolchain built them.
--INI--
phar.readonly=0
--SKIPIF--
<?php
/* php on Windows names one archive by the 8.3 short temp path in some
 * sentences and the long one, either slash, in others */
if (PHP_OS_FAMILY === 'Windows') {
    die("skip the archive paths pinned here are POSIX's\n");
}
--FILE--
<?php
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-phar-' . getmypid();
@mkdir($dir);
set_error_handler(function ($n, $s) use (&$dir) {
    echo '  [', $n, '] ', str_replace([realpath($dir) ?: $dir, $dir], '<dir>', $s), "\n";
    return true;
});
function show($label, $cb) {
    global $dir;
    echo $label, ': ';
    try { echo var_export($cb(), true), "\n"; }
    catch (Throwable $e) {
        echo get_class($e), ': ', str_replace([realpath($dir) ?: $dir, $dir], '<dir>', $e->getMessage()), "\n";
    }
}
$p = $dir . '/t.phar';

echo "-- an archive that does not exist yet is a new one\n";
var_dump(Phar::canWrite(), file_exists($p));
$ph = new Phar($p);
var_dump(count($ph), $ph->isFileFormat(Phar::PHAR), $ph->isFileFormat(Phar::TAR));

echo "-- adding entries writes the file\n";
$ph->addFromString('a.txt', 'AAA');
$ph['b/c.txt'] = 'BBB';
$ph->addFromString('b/d/e.txt', str_repeat('E', 500));
var_dump(count($ph), file_exists($p));
var_dump(file_get_contents("phar://$p/a.txt"), file_get_contents("phar://$p/b/c.txt"));
var_dump(strlen(file_get_contents("phar://$p/b/d/e.txt")));

echo "-- the entries are a DIRECTORY tree, and the wrapper walks it\n";
var_dump(is_file("phar://$p/a.txt"), is_dir("phar://$p/b"), is_dir("phar://$p/b/d"),
    file_exists("phar://$p/nope"), filesize("phar://$p/a.txt"));
var_dump(scandir("phar://$p"), scandir("phar://$p/b"));
echo "-- ...and so does the OBJECT, which is a directory iterator over itself\n";
/* Read through a FRESH object: php's live one stops iterating once it has been
 * written to (its own directory cache), and what is being measured here is the
 * archive rather than that. */
$it = new Phar($p);
foreach ($it as $key => $info) {
    printf("  %-8s dir=%d %s\n", $info->getFilename(), (int) $info->isDir(),
        str_replace([realpath($dir) ?: $dir, $dir], '<dir>', $key));
}
$flat = [];
foreach (new RecursiveIteratorIterator(new Phar($p)) as $info) { $flat[] = $info->getFilename(); }
sort($flat);
var_dump($flat);

echo "-- what an entry answers\n";
$fi = $ph['a.txt'];
var_dump(get_class($fi), $fi->getFilename(), $fi->getSize(), $fi->getContent(),
    $fi->isFile(), $fi->isDir(), $fi->isCompressed(), dechex($fi->getCRC32()),
    $fi->getCompressedSize(), $fi->getPharFlags());
show('a name that is not there', fn() => $ph['nope']);
var_dump($ph->offsetExists('a.txt'), $ph->offsetExists('nope'), isset($ph['b/c.txt']));

echo "-- metadata, both levels, php-serialized inside the manifest\n";
var_dump($ph->hasMetadata(), $ph->getMetadata());
$ph->setMetadata(['built' => 'here', 'n' => 3]);
$ph['a.txt']->setMetadata([1, 2, 3]);
var_dump($ph->hasMetadata(), $ph->getMetadata(), $ph['a.txt']->getMetadata());

echo "-- the stub, and the signature over everything before it\n";
$ph->setStub('<?php Phar::mapPhar("t.phar"); require "phar://t.phar/main.php"; __HALT_COMPILER();');
/* php appends ` ?>` and a CRLF to a stub that does not close itself; spelled with
 * an escape rather than a literal CR, which no source file should carry. */
var_dump(str_replace(["\r", "\n"], ['\\r', '\\n'], $ph->getStub()));
var_dump($ph->getSignature()['hash_type'], strlen($ph->getSignature()['hash']));
$ph->setSignatureAlgorithm(Phar::SHA512);
var_dump($ph->getSignature()['hash_type'], strlen($ph->getSignature()['hash']));
show('an algorithm php does not have', fn() => $ph->setSignatureAlgorithm(99));

echo "-- delete, copy and extract\n";
$ph->addFromString('main.php', '<?php echo "ran from ", basename(Phar::running()), "\n"; return 42;');
$ph->copy('a.txt', 'a2.txt');
var_dump(file_get_contents("phar://$p/a2.txt"));
var_dump($ph->delete('a2.txt'), $ph->offsetExists('a2.txt'));
show('deleting a name that is not there', fn() => $ph->delete('gone.txt'));
var_dump($ph->extractTo($dir . '/out', null, true));
var_dump(file_get_contents($dir . '/out/a.txt'), file_get_contents($dir . '/out/b/c.txt'));

echo "-- an archive is a PROGRAM: the file runs, mapPhar names it, running() answers\n";
unset($ph);
$out = [];
exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($p) . ' 2>&1', $out);
echo implode("\n", $out), "\n";
var_dump(include("phar://$p/main.php"));

echo "-- compression is per ENTRY, and reading is transparent\n";
$ph = new Phar($p);
$ph->compressFiles(Phar::GZ);
var_dump($ph['a.txt']->isCompressed(), $ph['a.txt']->isCompressed(Phar::GZ),
    file_get_contents("phar://$p/a.txt"), strlen(file_get_contents("phar://$p/b/d/e.txt")));
/* Only the FLAG is pinned on the way back: php 8.5's decompressFiles() corrupts
 * the entries it rewrites (`AAA` comes back as `stt`), and reproducing a defect
 * that loses data is not reproducing a contract. This engine's answer is the
 * original bytes. */
$ph->decompressFiles();
var_dump($ph['a.txt']->isCompressed());

echo "-- the same reader over a TAR and a ZIP, which is what PharData is\n";
foreach (['d.tar', 'd.zip'] as $name) {
    $q = $dir . '/' . $name;
    $pd = new PharData($q);
    $pd->addFromString('one.txt', 'ONE');
    $pd->addFromString('sub/two.txt', 'TWO');
    printf("  %s: count=%d tar=%d zip=%d phar=%d\n", $name, count($pd),
        (int) $pd->isFileFormat(Phar::TAR), (int) $pd->isFileFormat(Phar::ZIP),
        (int) $pd->isFileFormat(Phar::PHAR));
    var_dump(file_get_contents("phar://$q/one.txt"), file_get_contents("phar://$q/sub/two.txt"));
    /* Re-opened from disk, which is what proves the WRITER and the READER agree. */
    $re = new PharData($q);
    var_dump(count($re), $re['sub/two.txt']->getContent(), $re->getStub());
    show("$name stub", fn() => $re->setStub('<?php __HALT_COMPILER();'));
}
echo "-- a file that is no archive at all\n";
file_put_contents($dir . '/plain.txt', "not an archive\n");
show('new Phar(plain)', fn() => new Phar($dir . '/plain.txt'));
show('new PharData(plain)', fn() => new PharData($dir . '/plain.txt'));
show('a name with no known extension', fn() => new Phar($dir . '/x.dat'));

function rmtree(string $d): void {
    foreach (glob($d . '/*') as $f) { is_dir($f) ? rmtree($f) : @unlink($f); }
    @rmdir($d);
}
rmtree($dir);
--EXPECT--
-- an archive that does not exist yet is a new one
bool(true)
bool(false)
int(0)
bool(true)
bool(false)
-- adding entries writes the file
int(3)
bool(true)
string(3) "AAA"
string(3) "BBB"
int(500)
-- the entries are a DIRECTORY tree, and the wrapper walks it
bool(true)
bool(true)
bool(true)
bool(false)
int(3)
array(2) {
  [0]=>
  string(5) "a.txt"
  [1]=>
  string(1) "b"
}
array(2) {
  [0]=>
  string(5) "c.txt"
  [1]=>
  string(1) "d"
}
-- ...and so does the OBJECT, which is a directory iterator over itself
  a.txt    dir=0 phar://<dir>/t.phar/a.txt
  b        dir=1 phar://<dir>/t.phar/b
array(3) {
  [0]=>
  string(5) "a.txt"
  [1]=>
  string(5) "c.txt"
  [2]=>
  string(5) "e.txt"
}
-- what an entry answers
string(12) "PharFileInfo"
string(5) "a.txt"
int(3)
string(3) "AAA"
bool(true)
bool(false)
bool(false)
string(8) "66a031a7"
int(3)
int(0)
a name that is not there: BadMethodCallException: Entry nope does not exist
bool(true)
bool(false)
bool(true)
-- metadata, both levels, php-serialized inside the manifest
bool(false)
NULL
bool(true)
array(2) {
  ["built"]=>
  string(4) "here"
  ["n"]=>
  int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
-- the stub, and the signature over everything before it
string(90) "<?php Phar::mapPhar("t.phar"); require "phar://t.phar/main.php"; __HALT_COMPILER(); ?>\r\n"
string(7) "SHA-256"
int(64)
string(7) "SHA-512"
int(128)
an algorithm php does not have: UnexpectedValueException: Unknown signature algorithm specified
-- delete, copy and extract
string(3) "AAA"
bool(true)
bool(false)
deleting a name that is not there: BadMethodCallException: Entry gone.txt does not exist and cannot be deleted
bool(true)
string(3) "AAA"
string(3) "BBB"
-- an archive is a PROGRAM: the file runs, mapPhar names it, running() answers
ran from t.phar
ran from t.phar
int(42)
-- compression is per ENTRY, and reading is transparent
bool(true)
bool(true)
string(3) "AAA"
int(500)
bool(false)
-- the same reader over a TAR and a ZIP, which is what PharData is
  d.tar: count=2 tar=1 zip=0 phar=0
string(3) "ONE"
string(3) "TWO"
int(2)
string(3) "TWO"
string(0) ""
d.tar stub: UnexpectedValueException: A Phar stub cannot be set in a plain tar archive
  d.zip: count=2 tar=0 zip=1 phar=0
string(3) "ONE"
string(3) "TWO"
int(2)
string(3) "TWO"
string(0) ""
d.zip stub: UnexpectedValueException: A Phar stub cannot be set in a plain zip archive
-- a file that is no archive at all
new Phar(plain): UnexpectedValueException: Cannot create phar '<dir>/plain.txt', file extension (or combination) not recognised or the directory does not exist
new PharData(plain): UnexpectedValueException: internal corruption of phar "<dir>/plain.txt" (truncated entry)
a name with no known extension: UnexpectedValueException: Cannot create phar '<dir>/x.dat', file extension (or combination) not recognised or the directory does not exist
