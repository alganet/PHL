--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SplFileObject holds one line of look-ahead and picks between its two current values
--DESCRIPTION--
php's SPL_FS_FILE arm of spl_filesystem_object is an OPEN stream plus ONE line of
look-ahead, and nearly every rule is about which of the two current values is live
rather than about IO: current_line (a string) and current_zval (the READ_CSV array)
sit side by side, either or both present, and current() picks between them. The line
NUMBER is advanced by whatever read produced them, which is why key() counts
differently for fgetc() (only a newline advances it), for fgets() (always) and for
current() (only when a line was already there). The class was missing outright, so
every name here was a `Class "SplFileObject" not found` fatal.
--FILE--
<?php
function sfoShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', preg_replace('/#\d+/', '#N', str_replace("\n", '', $out)), "\n";
}
$sfoDir = sys_get_temp_dir() . '/phl_fileobject_' . getmypid();
@mkdir($sfoDir);
$sfoTxt = "$sfoDir/lines.txt";
$sfoCsv = "$sfoDir/rows.csv";
file_put_contents($sfoTxt, "a\nbb\n\nccc\n");
file_put_contents($sfoCsv, "a,b,c\n\"x,y\",z\n\n1,2\n");

/* The four refusals a constructor has, and the shape of a good one. */
sfoShow('a missing file raises', function () use ($sfoDir) {
    try { return new SplFileObject("$sfoDir/nope.txt"); }
    catch (RuntimeException $e) { return str_replace($sfoDir, '<dir>', $e->getMessage()); }
});
sfoShow('a directory is a LogicException', function () use ($sfoDir) {
    try { return new SplFileObject($sfoDir); } catch (LogicException $e) { return $e->getMessage(); }
});
sfoShow('an empty path', fn() => new SplFileObject(''));
sfoShow('a NUL in the path', fn() => new SplFileObject("x\0y"));
sfoShow('a second constructor call', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->__construct($sfoTxt);
});
sfoShow('the inherited accessors', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt);
    return [$f->getFilename(), $f->getExtension(), $f->getPath() === dirname($sfoTxt), $f->getSize()];
});
sfoShow('it is not cloneable', function () use ($sfoTxt) { return clone new SplFileObject($sfoTxt); });
sfoShow('it does not serialize', function () use ($sfoTxt) { return serialize(new SplFileObject($sfoTxt)); });
sfoShow('the interfaces it declares', fn() => array_values(array_intersect(
    ['Iterator', 'SeekableIterator', 'RecursiveIterator', 'Stringable'],
    class_implements('SplFileObject'))));
sfoShow('the four flag constants', fn() => [
    SplFileObject::DROP_NEW_LINE, SplFileObject::READ_AHEAD,
    SplFileObject::SKIP_EMPTY, SplFileObject::READ_CSV]);

/* An object whose parent constructor never ran answers NO method at all --
 * php gives this class a get_method handler, so the inherited ones stop too. */
class SfoNoParent extends SplFileObject { public function __construct($f) {} }
$sfoBare = new SfoNoParent('x');
foreach (['fgets', 'key', 'current', 'getFilename', 'getSize', 'getPathname'] as $sfoM) {
    sfoShow("bare->$sfoM", function () use ($sfoBare, $sfoM) { return $sfoBare->$sfoM(); });
}

/* The walk, flag by flag. The trailing row is the empty read past the last line:
 * valid() asks the STREAM, which is not at EOF until a read has found nothing. */
foreach ([0, SplFileObject::DROP_NEW_LINE,
          SplFileObject::DROP_NEW_LINE | SplFileObject::SKIP_EMPTY,
          SplFileObject::SKIP_EMPTY, SplFileObject::READ_AHEAD,
          SplFileObject::READ_AHEAD | SplFileObject::DROP_NEW_LINE] as $sfoFlags) {
    $f = new SplFileObject($sfoTxt);
    $f->setFlags($sfoFlags);
    $rows = [];
    foreach ($f as $k => $v) { $rows[] = $k . ':' . var_export($v, true); }
    echo 'flags ', str_pad((string)$sfoFlags, 2), ' => ', implode(' ', $rows), "\n";
}
sfoShow('setFlags keeps only its own bits', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->setFlags(0xFF0); return $f->getFlags();
});

/* key() counts what the READ did, not what the file holds. */
sfoShow('current does not advance a fresh object', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt);
    return [$f->key(), $f->current(), $f->key(), $f->fgets(), $f->key(), $f->current(), $f->key()];
});
sfoShow('fgetc counts newlines', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $out = [];
    for ($i = 0; $i < 4; $i++) { $out[] = $f->fgetc() . '/' . $f->key(); }
    return $out;
});
sfoShow('fgets past the end raises', function () use ($sfoTxt, $sfoDir) {
    $f = new SplFileObject($sfoTxt);
    while (!$f->eof()) { $f->fgets(); }
    try { $f->fgets(); } catch (RuntimeException $e) {
        return str_replace($sfoDir, '<dir>', $e->getMessage());
    }
});
sfoShow('seek walks forward', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->seek(2);
    return [$f->key(), $f->current()];
});
sfoShow('seek past the end', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->seek(100);
    return [$f->key(), $f->current(), $f->eof(), $f->valid()];
});
sfoShow('seek refuses a negative line', function () use ($sfoTxt) {
    (new SplFileObject($sfoTxt))->seek(-1);
});
sfoShow('__toString is the current line', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $a = (string)$f; $f->next(); return [$a, (string)$f];
});
sfoShow('maxLineLen caps every read', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->setMaxLineLen(2);
    return [$f->getMaxLineLen(), $f->fgets(), $f->fgets(), $f->fgets()];
});
sfoShow('maxLineLen refuses a negative', function () use ($sfoTxt) {
    (new SplFileObject($sfoTxt))->setMaxLineLen(-1);
});

/* READ_CSV: the array is the current value, and an empty line is [null]. */
sfoShow('READ_CSV iterates arrays', function () use ($sfoCsv) {
    $f = new SplFileObject($sfoCsv); $f->setFlags(SplFileObject::READ_CSV);
    return iterator_to_array($f);
});
sfoShow('READ_CSV|SKIP_EMPTY|DROP_NEW_LINE', function () use ($sfoCsv) {
    $f = new SplFileObject($sfoCsv);
    $f->setFlags(SplFileObject::READ_CSV | SplFileObject::SKIP_EMPTY | SplFileObject::DROP_NEW_LINE);
    return iterator_to_array($f);
});
sfoShow('fgetcsv walks the same rows', function () use ($sfoCsv) {
    $f = new SplFileObject($sfoCsv); $out = [];
    for ($i = 0; $i < 5; $i++) { $out[] = $f->fgetcsv(',', '"', '\\'); }
    return $out;
});
sfoShow('the csv control round-trips', function () use ($sfoCsv) {
    $f = new SplFileObject($sfoCsv);
    $before = $f->getCsvControl();
    $f->setCsvControl(';', "'", '');
    return [$before, $f->getCsvControl()];
});
sfoShow('setCsvControl refuses a two-byte separator', function () use ($sfoCsv) {
    (new SplFileObject($sfoCsv))->setCsvControl(',,');
});
sfoShow('fgetcsv refuses one at ITS position', function () use ($sfoCsv) {
    (new SplFileObject($sfoCsv))->fgetcsv(',', 'ab', '\\');
});
sfoShow('fputcsv refuses one at ITS position', function () use ($sfoCsv) {
    (new SplFileObject($sfoCsv))->fputcsv(['a'], ',', 'ab', '\\');
});

/* Writing. The file is finished when the OBJECT dies. */
sfoShow('fwrite and its length rule', function () use ($sfoDir) {
    $p = "$sfoDir/out.txt";
    $f = new SplFileObject($p, 'w');
    $n = [$f->fwrite('hello'), $f->fwrite('world', 3), $f->fwrite('x', -1), $f->fwrite('')];
    unset($f);
    return [$n, file_get_contents($p)];
});
sfoShow('fputcsv writes a row', function () use ($sfoDir) {
    $p = "$sfoDir/out2.csv";
    $f = new SplFileObject($p, 'w');
    $n = $f->fputcsv(['a', 'b,c'], ',', '"', '\\', "\n");
    unset($f);
    return [$n, file_get_contents($p)];
});
sfoShow('ftruncate refuses a negative size', function () use ($sfoDir) {
    (new SplFileObject("$sfoDir/out.txt", 'r+'))->ftruncate(-1);
});
sfoShow('fread refuses a non-positive length', function () use ($sfoTxt) {
    (new SplFileObject($sfoTxt))->fread(0);
});
sfoShow('flock names ITS argument', function () use ($sfoTxt) {
    (new SplFileObject($sfoTxt))->flock(0);
});
sfoShow('flock writes the out-param', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt);
    $ok = $f->flock(LOCK_SH | LOCK_NB, $wouldBlock);
    return [$ok, $wouldBlock, $f->flock(LOCK_UN)];
});
sfoShow('the stream plumbing', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt);
    return [$f->ftell(), $f->fread(2), $f->ftell(), $f->fseek(0), $f->ftell(),
            isset($f->fstat()['size']), $f->eof()];
});
sfoShow('fscanf reads the next line', function () use ($sfoDir) {
    $p = "$sfoDir/scan.txt";
    file_put_contents($p, "12 ab\n34 cd\n");
    $f = new SplFileObject($p);
    $a = $f->fscanf('%d %s');
    $n = $f->fscanf('%d %s', $i, $s);
    return [$a, $n, $i, $s];
});
sfoShow('fpassthru prints the rest', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); $f->fgets();
    ob_start(); $n = $f->fpassthru(); $printed = ob_get_clean();
    return [$n, $printed];
});

/* php's RecursiveIterator half: a file has no children and says so. */
sfoShow('hasChildren/getChildren', function () use ($sfoTxt) {
    $f = new SplFileObject($sfoTxt); return [$f->hasChildren(), $f->getChildren()];
});

/* A subclass that overrides getCurrentLine() DRIVES the walk, and the line
 * number is bumped once by the read it made and again by the caller. */
class SfoBracket extends SplFileObject {
    public function getCurrentLine(): string { return '[' . parent::getCurrentLine() . ']'; }
}
sfoShow('an overridden getCurrentLine drives the walk', function () use ($sfoTxt) {
    $f = new SfoBracket($sfoTxt); return iterator_to_array($f);
});
class SfoBadLine extends SplFileObject {
    public function getCurrentLine(): string { return 'x'; }
}
sfoShow('a fixed getCurrentLine repeats forever', function () use ($sfoTxt) {
    $f = new SfoBadLine($sfoTxt); $out = [];
    foreach ($f as $k => $v) { $out[$k] = $v; if (count($out) > 3) { break; } }
    return $out;
});

/* Presentation: php declares no property here either, and shows five keys. */
$sfoOne = new SplFileObject($sfoTxt);
ob_start(); var_dump($sfoOne); $sfoDump = trim(ob_get_clean());
echo preg_replace(['/#\d+/', '/string\(\d+\) "<dir>/'], ['#N', 'string(N) "<dir>'],
    str_replace([$sfoDir, "\n"], ['<dir>', ' '], $sfoDump)), "\n";
sfoShow('debugInfo keys', fn() => array_keys($sfoOne->__debugInfo()));
sfoShow('cast is empty', fn() => (array)$sfoOne);
sfoShow('no declared properties', fn() => (new ReflectionClass('SplFileObject'))->getProperties());
unset($sfoOne);

/* A memory stream is a file like any other. */
sfoShow('php://memory', function () {
    $f = new SplFileObject('php://memory', 'w+');
    $f->fwrite("a\nb\n"); $f->rewind();
    return [$f->getPathname(), iterator_to_array($f)];
});

@unlink($sfoTxt); @unlink($sfoCsv); @unlink("$sfoDir/out.txt");
@unlink("$sfoDir/out2.csv"); @unlink("$sfoDir/scan.txt"); @rmdir($sfoDir);
--EXPECT--
a missing file raises => 'SplFileObject::__construct(<dir>/nope.txt): Failed to open stream: No such file or directory'
a directory is a LogicException => 'Cannot use SplFileObject with directories'
an empty path => ValueError: Path must not be empty
a NUL in the path => ValueError: SplFileObject::__construct(): Argument #N ($filename) must not contain any null bytes
a second constructor call => Error: Cannot call constructor twice
the inherited accessors => array (  0 => 'lines.txt',  1 => 'txt',  2 => true,  3 => 10,)
it is not cloneable => Error: Trying to clone an uncloneable object of class SplFileObject
it does not serialize => Exception: Serialization of 'SplFileObject' is not allowed
the interfaces it declares => array (  0 => 'Iterator',  1 => 'SeekableIterator',  2 => 'RecursiveIterator',  3 => 'Stringable',)
the four flag constants => array (  0 => 1,  1 => 2,  2 => 4,  3 => 8,)
bare->fgets => Error: The parent constructor was not called: the object is in an invalid state
bare->key => Error: The parent constructor was not called: the object is in an invalid state
bare->current => Error: The parent constructor was not called: the object is in an invalid state
bare->getFilename => Error: The parent constructor was not called: the object is in an invalid state
bare->getSize => Error: The parent constructor was not called: the object is in an invalid state
bare->getPathname => Error: The parent constructor was not called: the object is in an invalid state
flags 0  => 0:'a
' 1:'bb
' 2:'
' 3:'ccc
' 4:''
flags 1  => 0:'a' 1:'bb' 2:'' 3:'ccc' 4:''
flags 5  => 0:'a' 1:'bb' 2:'ccc' 3:false
flags 4  => 0:'a
' 1:'bb
' 2:'
' 3:'ccc
' 4:false
flags 2  => 0:'a
' 1:'bb
' 2:'
' 3:'ccc
' 4:''
flags 3  => 0:'a' 1:'bb' 2:'' 3:'ccc' 4:''
setFlags keeps only its own bits => 0
current does not advance a fresh object => array (  0 => 0,  1 => 'a',  2 => 0,  3 => 'bb',  4 => 1,  5 => 'bb',  6 => 1,)
fgetc counts newlines => array (  0 => 'a/0',  1 => '/1',  2 => 'b/1',  3 => 'b/1',)
fgets past the end raises => 'Cannot read from file <dir>/lines.txt'
seek walks forward => array (  0 => 2,  1 => '',)
seek past the end => array (  0 => 4,  1 => false,  2 => true,  3 => false,)
seek refuses a negative line => ValueError: SplFileObject::seek(): Argument #N ($line) must be greater than or equal to 0
__toString is the current line => array (  0 => 'a',  1 => 'bb',)
maxLineLen caps every read => array (  0 => 2,  1 => 'a',  2 => 'bb',  3 => '',)
maxLineLen refuses a negative => ValueError: SplFileObject::setMaxLineLen(): Argument #N ($maxLength) must be greater than or equal to 0
READ_CSV iterates arrays => array (  0 =>   array (    0 => 'a',    1 => 'b',    2 => 'c',  ),  1 =>   array (    0 => 'x,y',    1 => 'z',  ),  2 =>   array (    0 => NULL,  ),  3 =>   array (    0 => '1',    1 => '2',  ),  4 =>   array (    0 => NULL,  ),)
READ_CSV|SKIP_EMPTY|DROP_NEW_LINE => array (  0 =>   array (    0 => 'a',    1 => 'b',    2 => 'c',  ),  1 =>   array (    0 => 'x,y',    1 => 'z',  ),  3 =>   array (    0 => '1',    1 => '2',  ),  4 => false,)
fgetcsv walks the same rows => array (  0 =>   array (    0 => 'a',    1 => 'b',    2 => 'c',  ),  1 =>   array (    0 => 'x,y',    1 => 'z',  ),  2 =>   array (    0 => NULL,  ),  3 =>   array (    0 => '1',    1 => '2',  ),  4 =>   array (    0 => NULL,  ),)
the csv control round-trips => array (  0 =>   array (    0 => ',',    1 => '"',    2 => '\\',  ),  1 =>   array (    0 => ';',    1 => '\'',    2 => '',  ),)
setCsvControl refuses a two-byte separator => ValueError: SplFileObject::setCsvControl(): Argument #N ($separator) must be a single character
fgetcsv refuses one at ITS position => ValueError: SplFileObject::fgetcsv(): Argument #N ($enclosure) must be a single character
fputcsv refuses one at ITS position => ValueError: SplFileObject::fputcsv(): Argument #N ($enclosure) must be a single character
fwrite and its length rule => array (  0 =>   array (    0 => 5,    1 => 3,    2 => 0,    3 => 0,  ),  1 => 'hellowor',)
fputcsv writes a row => array (  0 => 8,  1 => 'a,"b,c"',)
ftruncate refuses a negative size => ValueError: SplFileObject::ftruncate(): Argument #N ($size) must be greater than or equal to 0
fread refuses a non-positive length => ValueError: SplFileObject::fread(): Argument #N ($length) must be greater than 0
flock names ITS argument => ValueError: SplFileObject::flock(): Argument #N ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN
flock writes the out-param => array (  0 => true,  1 => 0,  2 => true,)
the stream plumbing => array (  0 => 0,  1 => 'a',  2 => 2,  3 => 0,  4 => 0,  5 => true,  6 => false,)
fscanf reads the next line => array (  0 =>   array (    0 => 12,    1 => 'ab',  ),  1 => 2,  2 => 34,  3 => 'cd',)
fpassthru prints the rest => array (  0 => 8,  1 => 'bbccc',)
hasChildren/getChildren => array (  0 => false,  1 => NULL,)
an overridden getCurrentLine drives the walk => array (  2 => '[a]',  5 => '[bb]',  8 => '[]',  11 => '[ccc]',  14 => '[]',)
a fixed getCurrentLine repeats forever => array (  0 => 'x',  1 => 'x',  2 => 'x',  3 => 'x',)
object(SplFileObject)#N (5) {   ["pathName":"SplFileInfo":private]=>   string(N) "<dir>/lines.txt"   ["fileName":"SplFileInfo":private]=>   string(9) "lines.txt"   ["openMode":"SplFileObject":private]=>   string(1) "r"   ["delimiter":"SplFileObject":private]=>   string(1) ","   ["enclosure":"SplFileObject":private]=>   string(1) """ }
debugInfo keys => array (  0 => '' . "\0" . 'SplFileInfo' . "\0" . 'pathName',  1 => '' . "\0" . 'SplFileInfo' . "\0" . 'fileName',  2 => '' . "\0" . 'SplFileObject' . "\0" . 'openMode',  3 => '' . "\0" . 'SplFileObject' . "\0" . 'delimiter',  4 => '' . "\0" . 'SplFileObject' . "\0" . 'enclosure',)
cast is empty => array ()
no declared properties => array ()
php://memory => array (  0 => 'php://memory',  1 =>   array (    0 => 'a',    1 => 'b',    2 => '',  ),)
