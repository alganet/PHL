--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DirectoryIterator walks an OPEN directory, one entry at a time
--DESCRIPTION--
php's spl_filesystem_object holds an open directory stream and ONE entry
(u.dir.dirp / entry / index); the embedded PHP read the whole directory into an
array at construction, and every difference followed from that: rewind() re-opens
and sees a file created since, key() is the READ index (which keeps counting past
the end), seek() walks forward through the object's own valid()/next() so a
subclass override is obeyed, and clone opens the directory again at the same
index instead of sharing a cursor. The pathname is LAZY — path + slash + entry,
rebuilt after every read — so getPathname() answers '' once the walk has run out
while getSize() still stats the directory. The chunk also invented
DirectoryIterator::getFlags(), inherited SplFileInfo's __toString() where php
aliases getFilename(), and published FOLLOW_SYMLINKS as 512 and OTHER_MODE_MASK
as 12288 where php has 16384 and 28672.
--FILE--
<?php
$dirBase = rtrim(sys_get_temp_dir(), '/\\') . '/phl_dirit_' . getmypid();
@mkdir($dirBase);
@mkdir("$dirBase/sub");
file_put_contents("$dirBase/a.txt", 'a');
file_put_contents("$dirBase/sub/x.txt", 'x');

function dirShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
/* Sorted, because the ORDER a directory hands back its entries is the
 * filesystem's and not the class's. */
function dirRows(iterable $it, callable $row) {
    $out = [];
    foreach ($it as $k => $v) { $out[] = $row($k, $v); }
    sort($out);
    echo implode("\n", $out), "\n";
}

echo "-- DirectoryIterator: current() is \$this, key() is the read index\n";
dirRows(new DirectoryIterator($dirBase),
    fn($k, $v) => sprintf('%s dot=%d class=%s str=%s', $v->getFilename(), (int)$v->isDot(),
        get_class($v), (string)$v));

echo "-- the pathname is a slice of the DIRECTORY, and __toString is the entry\n";
$it = new DirectoryIterator($dirBase);
while ($it->valid() && $it->getFilename() !== 'a.txt') { $it->next(); }
dirShow('getPath', fn() => basename($it->getPath()));
dirShow('getPathname', fn() => basename($it->getPathname()));
dirShow('__toString', fn() => (string)$it);
dirShow('getExtension', fn() => $it->getExtension());
dirShow('getBasename(.txt)', fn() => $it->getBasename('.txt'));
dirShow('getFileInfo class', fn() => get_class($it->getFileInfo()));
dirShow('getFileInfo path', fn() => basename($it->getFileInfo()->getPath()));
dirShow('getSize', fn() => $it->getSize());

echo "-- past the end: valid() false, the index keeps counting, no entry to describe\n";
$end = new DirectoryIterator($dirBase);
$seen = 0;
while ($end->valid()) { $seen++; $end->next(); }
dirShow('valid', fn() => $end->valid());
dirShow('key is the count', fn() => $end->key() === $seen);
dirShow('getFilename', fn() => $end->getFilename());
dirShow('getPathname', fn() => $end->getPathname());
dirShow('isDot', fn() => $end->isDot());
dirShow('getFileInfo', fn() => $end->getFileInfo());
/* Nothing above has asked for a PATH, so the lazy name is still absent and the
 * debug array is one key short -- php's `if (intern->file_name)`. */
dirShow('debug keys before any path is built', fn() => array_map(fn($k) => str_replace("\0", '|', $k),
    array_keys($end->__debugInfo())));
/* The lazy name past the end is "<dir><slash>": unix stats that as the
 * directory itself, Windows refuses a trailing separator -- php's own stat
 * draws the same line, so only the unix answer is asserted. */
if (DIRECTORY_SEPARATOR === '/') {
    dirShow('getSize stats the directory', fn() => $end->getSize() > 0);
} else {
    echo "getSize stats the directory => true\n";
}

echo "-- seek() rewinds backwards and refuses past the end\n";
$sk = new DirectoryIterator($dirBase);
$sk->seek(2); dirShow('seek(2) key', fn() => $sk->key());
$sk->seek(1); dirShow('seek(1) key', fn() => $sk->key());
dirShow('seek(99)', fn() => $sk->seek(99));
dirShow('left standing past the end', fn() => [$sk->key() === $seen, $sk->valid()]);

echo "-- seek() drives the OBJECT's own valid()/next()\n";
class DirItSeekLog extends DirectoryIterator {
    public $log = '';
    public function rewind(): void { $this->log .= 'r'; parent::rewind(); }
    public function valid(): bool { $this->log .= 'v'; return parent::valid(); }
    public function next(): void { $this->log .= 'n'; parent::next(); }
}
$lg = new DirItSeekLog($dirBase);
$lg->seek(2);
dirShow('forward', fn() => $lg->log);
$lg->seek(0);
dirShow('backward rewinds first', fn() => $lg->log);

echo "-- clone opens the directory again at the same index\n";
$c1 = new DirectoryIterator($dirBase); $c1->next();
$c2 = clone $c1;
dirShow('same position', fn() => [$c1->key() === $c2->key(),
    $c1->getFilename() === $c2->getFilename()]);
$c2->next();
dirShow('independent cursors', fn() => [$c1->key(), $c2->key()]);

echo "-- rewind() re-reads the directory\n";
$w = new FilesystemIterator($dirBase);
$before = iterator_count($w);
file_put_contents("$dirBase/late.txt", 'l');
$w->rewind();
$after = 0;
foreach ($w as $ignored) { $after++; }
dirShow('sees the new file', fn() => $after === $before + 1);
unlink("$dirBase/late.txt");

echo "-- FilesystemIterator: the flag MASKS, and SKIP_DOTS only by default\n";
dirShow('default flags', fn() => (new FilesystemIterator($dirBase))->getFlags());
dirShow('RecursiveDirectoryIterator default', fn() => (new RecursiveDirectoryIterator($dirBase))->getFlags());
dirShow('constants', fn() => [FilesystemIterator::FOLLOW_SYMLINKS,
    FilesystemIterator::OTHER_MODE_MASK, FilesystemIterator::CURRENT_MODE_MASK,
    FilesystemIterator::KEY_MODE_MASK, FilesystemIterator::NEW_CURRENT_AND_KEY]);
$fl = new FilesystemIterator($dirBase);
$fl->setFlags(PHP_INT_MAX);
dirShow('setFlags keeps the three fields', fn() => $fl->getFlags());
dirShow('passing flags drops SKIP_DOTS',
    fn() => iterator_count(new FilesystemIterator($dirBase, FilesystemIterator::CURRENT_AS_SELF))
          - iterator_count(new FilesystemIterator($dirBase)));

echo "-- current()/key() by mode\n";
dirRows(new FilesystemIterator($dirBase),
    fn($k, $v) => basename($k) . ' => ' . get_class($v) . '(' . $v->getFilename() . ')');
dirRows(new FilesystemIterator($dirBase,
        FilesystemIterator::KEY_AS_FILENAME | FilesystemIterator::CURRENT_AS_PATHNAME
        | FilesystemIterator::SKIP_DOTS),
    fn($k, $v) => $k . ' => ' . basename($v));
dirRows(new FilesystemIterator($dirBase,
        FilesystemIterator::CURRENT_AS_SELF | FilesystemIterator::SKIP_DOTS),
    fn($k, $v) => get_class($v) . ' ' . $v->getFilename());

echo "-- RecursiveDirectoryIterator: the sub path is the nested route\n";
$rd = new RecursiveDirectoryIterator($dirBase, FilesystemIterator::SKIP_DOTS);
foreach ($rd as $entry) {
    if ($rd->getFilename() !== 'sub') { continue; }
    dirShow('hasChildren', fn() => $rd->hasChildren());
    /* Normalized: php's sub-path separator is the platform's. */
    dirShow('subPath/subPathname', fn() => [strtr($rd->getSubPath(), '\\', '/'),
        strtr($rd->getSubPathname(), '\\', '/')]);
    $kid = $rd->getChildren();
    dirShow('child class/flags', fn() => [get_class($kid), $kid->getFlags()]);
    foreach ($kid as $leaf) {
        dirShow('leaf subPath/subPathname', fn() => [strtr($kid->getSubPath(), '\\', '/'),
            strtr($kid->getSubPathname(), '\\', '/')]);
    }
}
dirShow('a dot has no children',
    fn() => (new RecursiveDirectoryIterator($dirBase))->hasChildren());

echo "-- the classes declare no property, and show php's debug keys\n";
dirShow('properties', fn() => array_map(fn($p) => $p->getName(),
    (new ReflectionClass('RecursiveDirectoryIterator'))->getProperties()));
dirShow('cast', fn() => (array)new DirectoryIterator($dirBase));
dirShow('object vars', fn() => get_object_vars(new DirectoryIterator($dirBase)));
dirShow('debug keys', fn() => array_map(fn($k) => str_replace("\0", '|', $k),
    array_keys((new DirectoryIterator($dirBase))->__debugInfo())));
dirShow('serialize', fn() => serialize(new DirectoryIterator($dirBase)));

echo "-- refusals\n";
dirShow('empty path', fn() => new DirectoryIterator(''));
dirShow('empty path, subclass ctor', fn() => new RecursiveDirectoryIterator(''));
dirShow('not a directory', function () use ($dirBase) {
    try { return new DirectoryIterator("$dirBase/a.txt"); }
    catch (UnexpectedValueException $e) {
        /* The reason after the colon is the platform's own text (Windows php says
         * "The directory name is inval (code: 267)"); the rest is php's. */
        throw new UnexpectedValueException(str_replace("$dirBase/a.txt", '<path>', $e->getMessage()));
    }
});
dirShow('twice', function () use ($dirBase) {
    $d = new DirectoryIterator($dirBase);
    $d->__construct($dirBase);
});
class DirItNoParent extends DirectoryIterator { public function __construct() {} }
$np = new DirItNoParent();
foreach (['valid', 'key', 'current', 'next', 'rewind', 'getFilename', 'isDot',
          'getSize', 'isFile', 'getType', 'getFileInfo'] as $m) {
    dirShow("uninitialized $m", fn() => $np->$m());
}
dirShow('uninitialized getPathname', fn() => $np->getPathname());
dirShow('uninitialized getPathInfo', fn() => $np->getPathInfo());
dirShow('uninitialized debug keys', fn() => array_map(fn($k) => str_replace("\0", '|', $k),
    array_keys($np->__debugInfo())));

unlink("$dirBase/sub/x.txt");
unlink("$dirBase/a.txt");
rmdir("$dirBase/sub");
rmdir($dirBase);
?>
--EXPECTF--
-- DirectoryIterator: current() is $this, key() is the read index
. dot=1 class=DirectoryIterator str=.
.. dot=1 class=DirectoryIterator str=..
a.txt dot=0 class=DirectoryIterator str=a.txt
sub dot=0 class=DirectoryIterator str=sub
-- the pathname is a slice of the DIRECTORY, and __toString is the entry
getPath => '%s'
getPathname => 'a.txt'
__toString => 'a.txt'
getExtension => 'txt'
getBasename(.txt) => 'a'
getFileInfo class => 'SplFileInfo'
getFileInfo path => '%s'
getSize => 1
-- past the end: valid() false, the index keeps counting, no entry to describe
valid => false
key is the count => true
getFilename => ''
getPathname => ''
isDot => false
getFileInfo => RuntimeException: Could not open file
debug keys before any path is built => array (  0 => '|SplFileInfo|pathName',  1 => '|DirectoryIterator|glob',  2 => '|RecursiveDirectoryIterator|subPathName',)
getSize stats the directory => true
-- seek() rewinds backwards and refuses past the end
seek(2) key => 2
seek(1) key => 1
seek(99) => OutOfBoundsException: Seek position 99 is out of range
left standing past the end => array (  0 => true,  1 => false,)
-- seek() drives the OBJECT's own valid()/next()
forward => 'vnvn'
backward rewinds first => 'vnvnr'
-- clone opens the directory again at the same index
same position => array (  0 => true,  1 => true,)
independent cursors => array (  0 => 1,  1 => 2,)
-- rewind() re-reads the directory
sees the new file => true
-- FilesystemIterator: the flag MASKS, and SKIP_DOTS only by default
default flags => 4096
RecursiveDirectoryIterator default => 0
constants => array (  0 => 16384,  1 => 28672,  2 => 240,  3 => 3840,  4 => 256,)
setFlags keeps the three fields => 32752
passing flags drops SKIP_DOTS => 2
-- current()/key() by mode
a.txt => SplFileInfo(a.txt)
sub => SplFileInfo(sub)
a.txt => a.txt
sub => sub
FilesystemIterator a.txt
FilesystemIterator sub
-- RecursiveDirectoryIterator: the sub path is the nested route
hasChildren => true
subPath/subPathname => array (  0 => '',  1 => 'sub',)
child class/flags => array (  0 => 'RecursiveDirectoryIterator',  1 => 4096,)
leaf subPath/subPathname => array (  0 => 'sub',  1 => 'sub/x.txt',)
a dot has no children => false
-- the classes declare no property, and show php's debug keys
properties => array ()
cast => array ()
object vars => array ()
debug keys => array (  0 => '|SplFileInfo|pathName',  1 => '|SplFileInfo|fileName',  2 => '|DirectoryIterator|glob',  3 => '|RecursiveDirectoryIterator|subPathName',)
serialize => Exception: Serialization of 'DirectoryIterator' is not allowed
-- refusals
empty path => ValueError: DirectoryIterator::__construct(): Argument #1 ($directory) must not be empty
empty path, subclass ctor => ValueError: RecursiveDirectoryIterator::__construct(): Argument #1 ($directory) must not be empty
not a directory => UnexpectedValueException: DirectoryIterator::__construct(<path>): %A
twice => Error: Directory object is already initialized
uninitialized valid => Error: Object not initialized
uninitialized key => Error: Object not initialized
uninitialized current => Error: Object not initialized
uninitialized next => Error: Object not initialized
uninitialized rewind => Error: Object not initialized
uninitialized getFilename => Error: Object not initialized
uninitialized isDot => Error: Object not initialized
uninitialized getSize => Error: Object not initialized
uninitialized isFile => Error: Object not initialized
uninitialized getType => Error: Object not initialized
uninitialized getFileInfo => Error: Object not initialized
uninitialized getPathname => ''
uninitialized getPathInfo => NULL
uninitialized debug keys => array (  0 => '|SplFileInfo|pathName',)
