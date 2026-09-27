--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
openFile() builds the class setFileClass() named, and SplTempFileObject names a php:// URI
--DESCRIPTION--
The two doors into SplFileObject and the subclass that IS one. php's openFile() is
spl_filesystem_object_create_type for SPL_FS_FILE: it builds this instance's
file_class -- a slot of its own, so it survives a clone and travels to a directory
iterator's children -- and CALLS that class's constructor when the class declares one,
with the pathname AND the mode, which is how a subclass sees what it was opened as.
The path the child keeps is the SOURCE's rather than one re-derived from the name.
SplTempFileObject builds a php:// URI from its budget and opens it `wb`, and all three
arms are visible from outside because getPathname() answers the URI: a negative budget
is php://memory, a named one is php://temp/maxmemory:N (0 included), and no argument at
all is a plain php://temp. All three were `Class not found` / undefined-method fatals.
--FILE--
<?php
function sfdShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', preg_replace('/#\d+/', '#N', str_replace("\n", '', $out)), "\n";
}
$sfdDir = sys_get_temp_dir() . '/phl_fileobject_doors_' . getmypid();
@mkdir($sfdDir);
@mkdir("$sfdDir/sub");
$sfdTxt = "$sfdDir/lines.txt";
file_put_contents($sfdTxt, "a\nbb\n");

/* openFile(): the default class, the mode, and where the refusals come from. */
sfdShow('openFile answers an SplFileObject', function () use ($sfdTxt) {
    $o = (new SplFileInfo($sfdTxt))->openFile();
    return [get_class($o), $o->getFilename(), $o->fgets()];
});
sfdShow('openFile takes a mode', function () use ($sfdDir) {
    $p = "$sfdDir/written.txt";
    $o = (new SplFileInfo($p))->openFile('w');
    $o->fwrite('z');
    unset($o);
    return file_get_contents($p);
});
sfdShow('openFile promotes the open failure', function () use ($sfdDir) {
    try { return (new SplFileInfo("$sfdDir/nope.txt"))->openFile(); }
    catch (RuntimeException $e) { return str_replace($sfdDir, '<dir>', $e->getMessage()); }
});
sfdShow('openFile refuses a bad mode', function () use ($sfdDir, $sfdTxt) {
    try { return (new SplFileInfo($sfdTxt))->openFile('zz'); }
    catch (RuntimeException $e) { return str_replace($sfdDir, '<dir>', $e->getMessage()); }
});
sfdShow('openFile of a directory', function () use ($sfdDir) {
    (new SplFileInfo($sfdDir))->openFile();
});
sfdShow('openFile blames ITS context argument', function () use ($sfdTxt) {
    (new SplFileInfo($sfdTxt))->openFile('r', false, 'x');
});
sfdShow('and the constructor blames its own', function () use ($sfdTxt) {
    new SplFileObject($sfdTxt, 'r', false, 'x');
});
sfdShow('a context resource is accepted', function () use ($sfdTxt) {
    return get_class((new SplFileInfo($sfdTxt))->openFile('r', false, stream_context_create([])));
});

/* setFileClass(): the slot, its refusal, and what carries it. */
class SfdChild extends SplFileObject {}
class SfdSeen extends SplFileObject {
    public $seen;
    public function __construct($f, $m = 'r') { $this->seen = "$f|$m"; parent::__construct($f, $m); }
}
sfdShow('setFileClass decides what openFile builds', function () use ($sfdTxt) {
    $i = new SplFileInfo($sfdTxt);
    $i->setFileClass('SfdChild');
    return get_class($i->openFile());
});
sfdShow('and its default puts it back', function () use ($sfdTxt) {
    $i = new SplFileInfo($sfdTxt);
    $i->setFileClass('SfdChild');
    $i->setFileClass();
    return get_class($i->openFile());
});
sfdShow('a declared constructor is CALLED, with the mode', function () use ($sfdTxt) {
    $i = new SplFileInfo($sfdTxt);
    $i->setFileClass('SfdSeen');
    $o = $i->openFile('r');
    return [get_class($o), basename($o->seen), $o->fgets()];
});
sfdShow('setFileClass refuses a stranger', function () use ($sfdTxt) {
    (new SplFileInfo($sfdTxt))->setFileClass('stdClass');
});
sfdShow('a name that resolves to nothing', function () use ($sfdTxt) {
    (new SplFileInfo($sfdTxt))->setFileClass('SfdNoSuchClass');
});
sfdShow('and a null, which php stringifies', function () use ($sfdTxt) {
    (new SplFileInfo($sfdTxt))->setFileClass(null);
});
sfdShow('setInfoClass takes one the same way', function () use ($sfdTxt) {
    (new SplFileInfo($sfdTxt))->setInfoClass(null);
});
sfdShow('the slot survives a clone', function () use ($sfdTxt) {
    $i = new SplFileInfo($sfdTxt);
    $i->setFileClass('SfdChild');
    return get_class((clone $i)->openFile());
});
sfdShow('getFileInfo does NOT carry it', function () use ($sfdTxt) {
    $i = new SplFileInfo($sfdTxt);
    $i->setFileClass('SfdChild');
    return get_class($i->getFileInfo()->openFile());
});
sfdShow('a directory iterator carries it to its entries', function () use ($sfdDir) {
    $d = new DirectoryIterator($sfdDir);
    $d->setFileClass('SfdChild');
    foreach ($d as $e) { if ($e->isFile()) { return get_class($e->openFile()); } }
});
sfdShow('an entry keeps the walked directory as its path', function () use ($sfdDir) {
    $d = new FilesystemIterator($sfdDir);
    foreach ($d as $e) {
        if ($e->isFile()) { $o = $e->openFile(); return $o->getPath() === $sfdDir; }
    }
});
sfdShow('past the end there is no file to open', function () use ($sfdDir) {
    $d = new FilesystemIterator($sfdDir);
    foreach ($d as $e) {}
    $d->openFile();
});

/* SplTempFileObject: the three URIs, the mode, and the empty path. */
sfdShow('the three budgets name three URIs', fn() => [
    (new SplTempFileObject())->getPathname(),
    (new SplTempFileObject(0))->getPathname(),
    (new SplTempFileObject(64))->getPathname(),
    (new SplTempFileObject(-1))->getPathname(),
]);
sfdShow('the shape of one', function () {
    $o = new SplTempFileObject();
    return [get_class($o), $o->getPath(), $o->getFilename(), $o->getFlags(),
            $o->getRealPath(), $o->getBasename()];
});
sfdShow('it reads back what it wrote', function () {
    $o = new SplTempFileObject();
    $o->fwrite("a\nbb\n");
    $o->rewind();
    return [$o->fgets(), $o->key(), $o->fgets(), $o->key(), $o->eof()];
});
sfdShow('it is not cloneable either', fn() => clone new SplTempFileObject());
sfdShow('nor serializable', fn() => serialize(new SplTempFileObject()));
sfdShow('a second constructor call', function () { $o = new SplTempFileObject(); $o->__construct(); });
sfdShow('its parent constructor is checked too', function () {
    $c = new class() extends SplTempFileObject { public function __construct() {} };
    return $c->getFilename();
});
sfdShow('it is an SplFileObject', fn() => [
    is_subclass_of('SplTempFileObject', 'SplFileObject'),
    (new SplTempFileObject()) instanceof SplFileInfo,
]);
$sfdOne = new SplTempFileObject();
ob_start(); var_dump($sfdOne); $sfdDump = trim(ob_get_clean());
echo preg_replace('/#\d+/', '#N', str_replace("\n", ' ', $sfdDump)), "\n";
unset($sfdOne);

@unlink($sfdTxt); @unlink("$sfdDir/written.txt"); @rmdir("$sfdDir/sub"); @rmdir($sfdDir);
--EXPECT--
openFile answers an SplFileObject => array (  0 => 'SplFileObject',  1 => 'lines.txt',  2 => 'a',)
openFile takes a mode => 'z'
openFile promotes the open failure => 'SplFileInfo::openFile(<dir>/nope.txt): Failed to open stream: No such file or directory'
openFile refuses a bad mode => 'SplFileInfo::openFile(<dir>/lines.txt): Failed to open stream: `zz\' is not a valid mode for fopen'
openFile of a directory => LogicException: Cannot use SplFileObject with directories
openFile blames ITS context argument => TypeError: SplFileInfo::openFile(): Argument #N ($context) must be of type resource or null, string given
and the constructor blames its own => TypeError: SplFileObject::__construct(): Argument #N ($context) must be of type resource or null, string given
a context resource is accepted => 'SplFileObject'
setFileClass decides what openFile builds => 'SfdChild'
and its default puts it back => 'SplFileObject'
a declared constructor is CALLED, with the mode => array (  0 => 'SfdSeen',  1 => 'lines.txt|r',  2 => 'a',)
setFileClass refuses a stranger => TypeError: SplFileInfo::setFileClass(): Argument #N ($class) must be a class name derived from SplFileObject, stdClass given
a name that resolves to nothing => TypeError: SplFileInfo::setFileClass(): Argument #N ($class) must be a class name derived from SplFileObject, SfdNoSuchClass given
and a null, which php stringifies => TypeError: SplFileInfo::setFileClass(): Argument #N ($class) must be a class name derived from SplFileObject,  given
setInfoClass takes one the same way => TypeError: SplFileInfo::setInfoClass(): Argument #N ($class) must be a class name derived from SplFileInfo,  given
the slot survives a clone => 'SfdChild'
getFileInfo does NOT carry it => 'SplFileObject'
a directory iterator carries it to its entries => 'SfdChild'
an entry keeps the walked directory as its path => true
past the end there is no file to open => RuntimeException: Could not open file
the three budgets name three URIs => array (  0 => 'php://temp',  1 => 'php://temp/maxmemory:0',  2 => 'php://temp/maxmemory:64',  3 => 'php://memory',)
the shape of one => array (  0 => 'SplTempFileObject',  1 => '',  2 => 'php://temp',  3 => 0,  4 => false,  5 => 'temp',)
it reads back what it wrote => array (  0 => 'a',  1 => 1,  2 => 'bb',  3 => 2,  4 => true,)
it is not cloneable either => Error: Trying to clone an uncloneable object of class SplTempFileObject
nor serializable => Exception: Serialization of 'SplTempFileObject' is not allowed
a second constructor call => Error: Cannot call constructor twice
its parent constructor is checked too => Error: The parent constructor was not called: the object is in an invalid state
it is an SplFileObject => array (  0 => true,  1 => true,)
object(SplTempFileObject)#N (5) {   ["pathName":"SplFileInfo":private]=>   string(10) "php://temp"   ["fileName":"SplFileInfo":private]=>   string(10) "php://temp"   ["openMode":"SplFileObject":private]=>   string(2) "wb"   ["delimiter":"SplFileObject":private]=>   string(1) ","   ["enclosure":"SplFileObject":private]=>   string(1) """ }
