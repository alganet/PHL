--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SplFileInfo slices one stored pathname and raises where a stat fails
--DESCRIPTION--
php's spl_filesystem_object keeps TWO strings and which one a method reads is the
whole model: file_name is the pathname with trailing slashes stripped, and path is
everything before its LAST slash — empty when the name has no slash before its
final component, which is why (new SplFileInfo('/a.txt'))->getPath() is '' and
getFilename() answers the whole '/a.txt'. The embedded PHP called basename() and
dirname() per method instead and disagreed on all of it. The stat family is php's
FileInfoFunction macro: php_stat with the error handler REPLACED, so a failed stat
RAISES a RuntimeException instead of warning and answering false, and two of the
accessors lstat rather than stat. Eleven methods were missing outright, four
(__join/__load/__sync/__skipDots) were invented, and the two slots — private and
shown only through the debug handler — were protected and on every surface.
--FILE--
<?php
function sfiShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', preg_replace('/#\d+/', '#N', str_replace("\n", '', $out)), "\n";
}

/* One pathname, sliced. The path is a PREFIX of the name or it is empty. */
foreach (['/tmp/x.txt', '/a.txt', 'a.txt', '/', '//', '', '.', 'dir/', '/x/y/', 'a//b',
          '.hidden', '/a/.hidden', 'a.b.c'] as $sfiPath) {
    $f = new SplFileInfo($sfiPath);
    echo str_pad(var_export($sfiPath, true), 13), ' path=', str_pad(var_export($f->getPath(), true), 8),
         ' name=', str_pad(var_export($f->getFilename(), true), 10),
         ' base=', str_pad(var_export($f->getBasename(), true), 10),
         ' ext=', str_pad(var_export($f->getExtension(), true), 9),
         ' pathname=', var_export($f->getPathname(), true), "\n";
}
sfiShow('getBasename strips a suffix', fn() => (new SplFileInfo('/tmp/x.txt'))->getBasename('.txt'));
sfiShow('a suffix that IS the name stays', fn() => (new SplFileInfo('/tmp/.txt'))->getBasename('.txt'));
sfiShow('__toString is getPathname', fn() => (string)(new SplFileInfo('/x/y/')));
sfiShow('getPathInfo is the dirname', fn() => (new SplFileInfo('/a/b/c'))->getPathInfo()->getPathname());
sfiShow('getPathInfo of nothing is null', fn() => (new SplFileInfo(''))->getPathInfo());
sfiShow('getFileInfo is a copy', function () {
    $f = new SplFileInfo('/a/b'); $g = $f->getFileInfo();
    return [get_class($g), $g->getPathname(), $g->getPath()];
});

/* The two slots are PRIVATE and PRESENTED: no declared property, nothing on the
 * cast, and the mangled private keys under var_dump. */
$sfiOne = new SplFileInfo('/tmp/probe.txt');
ob_start(); var_dump($sfiOne); $sfiDump = trim(ob_get_clean());
echo preg_replace('/#\d+/', '#N', str_replace("\n", ' ', $sfiDump)), "\n";
sfiShow('debugInfo keys', fn() => array_keys($sfiOne->__debugInfo()));
sfiShow('cast is empty', fn() => (array)$sfiOne);
sfiShow('object vars are empty', fn() => get_object_vars($sfiOne));
sfiShow('no declared properties', fn() => (new ReflectionClass('SplFileInfo'))->getProperties());
sfiShow('it does not serialize', fn() => serialize($sfiOne));
sfiShow('it does clone', fn() => (clone $sfiOne)->getPathname());

/* A missing file: the predicates answer false and every stat RAISES. */
$sfiGone = new SplFileInfo(__DIR__ . '/no_such_file_here.txt');
sfiShow('isFile', fn() => [$sfiGone->isFile(), $sfiGone->isDir(), $sfiGone->isLink(),
                           $sfiGone->isReadable(), $sfiGone->isWritable(), $sfiGone->isExecutable()]);
sfiShow('getRealPath answers false', fn() => $sfiGone->getRealPath());
foreach (['getSize', 'getPerms', 'getInode', 'getOwner', 'getGroup',
          'getATime', 'getMTime', 'getCTime', 'getType'] as $sfiM) {
    sfiShow($sfiM . ' raises', function () use ($sfiGone, $sfiM) {
        try { return $sfiGone->$sfiM(); }
        catch (RuntimeException $e) {
            return get_class($e) . ': ' . str_replace(__DIR__, '<dir>', $e->getMessage());
        }
    });
}

/* A file that IS there, on a path this test owns. */
$sfiDir = sys_get_temp_dir() . '/phl_fileinfo_' . getmypid();
@mkdir($sfiDir);
file_put_contents("$sfiDir/data.bin", str_repeat('x', 11));
$sfiReal = new SplFileInfo("$sfiDir/data.bin");
sfiShow('getSize', fn() => $sfiReal->getSize());
sfiShow('getType', fn() => $sfiReal->getType());
sfiShow('the directory is a dir', fn() => (new SplFileInfo($sfiDir))->getType());
sfiShow('stat fields are ints', fn() => array_map('is_int', [
    $sfiReal->getPerms(), $sfiReal->getInode(), $sfiReal->getOwner(),
    $sfiReal->getGroup(), $sfiReal->getATime(), $sfiReal->getMTime(), $sfiReal->getCTime(),
]));
/* The resolved path is the platform's own spelling — the NAME is what is portable. */
sfiShow('getRealPath resolves', fn() => basename((string)$sfiReal->getRealPath()));
sfiShow('predicates', fn() => [$sfiReal->isFile(), $sfiReal->isDir(), $sfiReal->isLink(),
                               $sfiReal->isReadable(), $sfiReal->isWritable()]);
sfiShow('getLinkTarget on a plain file raises', function () use ($sfiReal, $sfiDir) {
    /* Windows has no readlink(2): php resolves the handle there, so a plain file
     * reads back as its own final path instead of raising. The unix line is the
     * one pinned; Windows prints it only when the file resolved to itself. */
    if (PHP_OS_FAMILY === 'Windows') {
        $t = $sfiReal->getLinkTarget();
        return strcasecmp(basename($t), 'data.bin') === 0 && is_file($t)
            ? 'RuntimeException: Unable to read link <dir>/data.bin, error: <errno>' : $t;
    }
    try { return $sfiReal->getLinkTarget(); }
    catch (RuntimeException $e) {
        /* The errno text is the platform's; the shape of the message is php's. */
        return get_class($e) . ': ' . preg_replace('/, error: .*$/', ', error: <errno>',
            str_replace($sfiDir, '<dir>', $e->getMessage()));
    }
});

/* setInfoClass()/getFileInfo($class): only a class derived from this one. */
class SfiChild extends SplFileInfo {}
sfiShow('getFileInfo takes a class', fn() => get_class($sfiReal->getFileInfo('SfiChild')));
sfiShow('getPathInfo takes a class', fn() => get_class($sfiReal->getPathInfo('SfiChild')));
sfiShow('setInfoClass sticks', function () use ($sfiDir) {
    $f = new SplFileInfo("$sfiDir/data.bin"); $f->setInfoClass('SfiChild');
    return [get_class($f->getFileInfo()), get_class($f->getPathInfo())];
});
sfiShow('setInfoClass refuses a stranger', fn() => (new SplFileInfo('x'))->setInfoClass('stdClass'));
sfiShow('getFileInfo refuses one too', fn() => (new SplFileInfo('x'))->getFileInfo('stdClass'));
sfiShow('a subclass constructor runs', function () use ($sfiDir) {
    eval('class SfiCounted extends SplFileInfo { public static $n = 0;
          public function __construct(string $p) { self::$n++; parent::__construct($p); } }');
    $f = new SplFileInfo("$sfiDir/data.bin");
    $g = $f->getFileInfo('SfiCounted');
    return [get_class($g), $g->getPathname() === "$sfiDir/data.bin", SfiCounted::$n];
});

/* php's own escape hatch for a subclass that never called the parent constructor.
 * It is only reachable by name — php DEPRECATES the call (since 8.2) and PHL keeps
 * it silent, the standing non-deprecated-compatibility policy, so what this pins is
 * the declaration rather than the call. */
sfiShow('_bad_state_ex is final and returns void', fn() => array_map(
    fn($m) => [$m->isFinal(), (string)$m->getReturnType()],
    [new ReflectionMethod('SplFileInfo', '_bad_state_ex')]));

@unlink("$sfiDir/data.bin");
@rmdir($sfiDir);
--EXPECT--
'/tmp/x.txt'  path='/tmp'   name='x.txt'    base='x.txt'    ext='txt'     pathname='/tmp/x.txt'
'/a.txt'      path=''       name='/a.txt'   base='a.txt'    ext='txt'     pathname='/a.txt'
'a.txt'       path=''       name='a.txt'    base='a.txt'    ext='txt'     pathname='a.txt'
'/'           path=''       name='/'        base=''         ext=''        pathname='/'
'//'          path=''       name='/'        base=''         ext=''        pathname='/'
''            path=''       name=''         base=''         ext=''        pathname=''
'.'           path=''       name='.'        base='.'        ext=''        pathname='.'
'dir/'        path=''       name='dir'      base='dir'      ext=''        pathname='dir'
'/x/y/'       path='/x'     name='y'        base='y'        ext=''        pathname='/x/y'
'a//b'        path='a/'     name='b'        base='b'        ext=''        pathname='a//b'
'.hidden'     path=''       name='.hidden'  base='.hidden'  ext='hidden'  pathname='.hidden'
'/a/.hidden'  path='/a'     name='.hidden'  base='.hidden'  ext='hidden'  pathname='/a/.hidden'
'a.b.c'       path=''       name='a.b.c'    base='a.b.c'    ext='c'       pathname='a.b.c'
getBasename strips a suffix => 'x'
a suffix that IS the name stays => '.txt'
__toString is getPathname => '/x/y'
getPathInfo is the dirname => '/a/b'
getPathInfo of nothing is null => NULL
getFileInfo is a copy => array (  0 => 'SplFileInfo',  1 => '/a/b',  2 => '/a',)
object(SplFileInfo)#N (2) {   ["pathName":"SplFileInfo":private]=>   string(14) "/tmp/probe.txt"   ["fileName":"SplFileInfo":private]=>   string(9) "probe.txt" }
debugInfo keys => array (  0 => '' . "\0" . 'SplFileInfo' . "\0" . 'pathName',  1 => '' . "\0" . 'SplFileInfo' . "\0" . 'fileName',)
cast is empty => array ()
object vars are empty => array ()
no declared properties => array ()
it does not serialize => Exception: Serialization of 'SplFileInfo' is not allowed
it does clone => '/tmp/probe.txt'
isFile => array (  0 => false,  1 => false,  2 => false,  3 => false,  4 => false,  5 => false,)
getRealPath answers false => false
getSize raises => 'RuntimeException: SplFileInfo::getSize(): stat failed for <dir>/no_such_file_here.txt'
getPerms raises => 'RuntimeException: SplFileInfo::getPerms(): stat failed for <dir>/no_such_file_here.txt'
getInode raises => 'RuntimeException: SplFileInfo::getInode(): stat failed for <dir>/no_such_file_here.txt'
getOwner raises => 'RuntimeException: SplFileInfo::getOwner(): stat failed for <dir>/no_such_file_here.txt'
getGroup raises => 'RuntimeException: SplFileInfo::getGroup(): stat failed for <dir>/no_such_file_here.txt'
getATime raises => 'RuntimeException: SplFileInfo::getATime(): stat failed for <dir>/no_such_file_here.txt'
getMTime raises => 'RuntimeException: SplFileInfo::getMTime(): stat failed for <dir>/no_such_file_here.txt'
getCTime raises => 'RuntimeException: SplFileInfo::getCTime(): stat failed for <dir>/no_such_file_here.txt'
getType raises => 'RuntimeException: SplFileInfo::getType(): Lstat failed for <dir>/no_such_file_here.txt'
getSize => 11
getType => 'file'
the directory is a dir => 'dir'
stat fields are ints => array (  0 => true,  1 => true,  2 => true,  3 => true,  4 => true,  5 => true,  6 => true,)
getRealPath resolves => 'data.bin'
predicates => array (  0 => true,  1 => false,  2 => false,  3 => true,  4 => true,)
getLinkTarget on a plain file raises => 'RuntimeException: Unable to read link <dir>/data.bin, error: <errno>'
getFileInfo takes a class => 'SfiChild'
getPathInfo takes a class => 'SfiChild'
setInfoClass sticks => array (  0 => 'SfiChild',  1 => 'SfiChild',)
setInfoClass refuses a stranger => TypeError: SplFileInfo::setInfoClass(): Argument #N ($class) must be a class name derived from SplFileInfo, stdClass given
getFileInfo refuses one too => TypeError: SplFileInfo::getFileInfo(): Argument #N ($class) must be a class name derived from SplFileInfo or null, stdClass given
a subclass constructor runs => array (  0 => 'SfiCounted',  1 => true,  2 => 1,)
_bad_state_ex is final and returns void => array (  0 =>   array (    0 => true,    1 => 'void',  ),)
