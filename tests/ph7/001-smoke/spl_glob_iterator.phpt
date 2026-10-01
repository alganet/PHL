--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
GlobIterator walks a pattern's matches through the directory machinery
--DESCRIPTION--
`new GlobIterator('*.php')` was a `Class not found` fatal -- the last name of
php's SPL file trio, and the last of the SPL surface. The class is the whole
DirectoryIterator machinery over a `glob://` stream, so almost none of it is new
code; what is its own is that the constructor PREFIXES the scheme (so the path
slot always holds `glob://pattern`, which is what the `glob` debug key shows and
why getPath() has to ask the STREAM instead), that its default flags are 0 --
SKIP_DOTS OFF where FilesystemIterator has it on -- that count() answers the
number of MATCHES, and that php gives it SplFileObject's `check` object handlers:
uncloneable, and EVERY method refused, inherited ones included, on an instance
whose parent constructor never ran. getPath() is the directory of the CURRENT
match and moves with the walk, so the SplFileInfo each step hands out is built
from that and not from the pattern.
--FILE--
<?php
/* php's GlobIterator is the DirectoryIterator machinery over a `glob://`
 * stream: the constructor prefixes the scheme when the caller did not write it,
 * and everything else -- rewind/valid/key/current/next/seek, the flags, the
 * lazy pathname -- is inherited unchanged. What is its own is where the PATH
 * comes from (the current match's directory, which the stream tracks, not the
 * pattern in the slot), that its default flags are 0 so SKIP_DOTS is OFF, that
 * count() answers the number of MATCHES, and that php gives it SplFileObject's
 * `check` handlers: uncloneable, and every method refused on an instance whose
 * parent constructor never ran. */
$giDir = sys_get_temp_dir() . '/phl_gi_' . getmypid();
@mkdir($giDir);
foreach (['s1', 's2'] as $giD) { @mkdir("$giDir/$giD"); }
foreach (['1.txt', '2.txt', '10.txt', 'z.log', '.hidden',
          's1/x.txt', 's2/y.txt'] as $giF) { file_put_contents("$giDir/$giF", ''); }
/* The lazy pathname is JOINED with the platform's separator -- php uses `\` on
 * Windows unless UNIX_PATHS is set, and so does this engine -- while the glob
 * PATH keeps whatever the pattern spelled. Both are normalised here so that
 * what the test pins is the names and the order, not the separator. */
function giClean($s) {
    global $giDir;
    return str_replace([$giDir, '\\'], ['<d>', '/'], (string)$s);
}

echo "class exists : ", var_export(class_exists('GlobIterator'), true), "\n";
$giR = new ReflectionClass('GlobIterator');
echo "parent       : ", $giR->getParentClass()->getName(), "\n";
/* Sorted: php's own getInterfaceNames() order is an artifact of how it links a
 * class (DirectoryIterator and FilesystemIterator come back in opposite orders
 * there), so what is pinned here is the SET. */
$giI = $giR->getInterfaceNames(); sort($giI);
echo "interfaces   : ", json_encode($giI), "\n";
echo "own methods  : ", json_encode(array_values(array_map(fn($m) => $m->getName(),
     array_filter($giR->getMethods(), fn($m) => $m->getDeclaringClass()->getName() === 'GlobIterator')))), "\n";

/* Default flags are 0: KEY_AS_PATHNAME|CURRENT_AS_FILEINFO, SKIP_DOTS OFF. */
$git = new GlobIterator("$giDir/*.txt");
echo "flags        : ", $git->getFlags(), "\n";
echo "count        : ", count($git), ' ', $git->count(), "\n";
foreach ($git as $giK => $giV) {
    echo '  ', giClean($giK), ' => ', get_class($giV), ' path=', giClean($giV->getPath()),
         ' file=', $giV->getFilename(), ' name=', giClean($giV->getPathname()), "\n";
}
echo "past end     : ", var_export($git->valid(), true), ' key=', var_export(giClean($git->key()), true),
     ' path=', var_export(giClean($git->getPath()), true),
     ' name=', var_export(giClean($git->getPathname()), true), "\n";

/* getPath() is the CURRENT match's directory and moves with the walk. */
$git = new GlobIterator("$giDir/s?/*.txt");
foreach ($git as $giV) { echo '  walk path=', giClean($giV->getPath()), ' file=', $giV->getFilename(), "\n"; }

/* SKIP_DOTS is off, so the dot entries a dot-pattern matches come through. */
$git = new GlobIterator("$giDir/.*");
echo "dots         : ", json_encode(array_map('giClean', array_keys(iterator_to_array($git)))), "\n";
$git = new GlobIterator("$giDir/.*", FilesystemIterator::SKIP_DOTS);
echo "dots skipped : ", json_encode(array_map('giClean', array_keys(iterator_to_array($git)))),
     ' count=', count($git), "\n";

/* A pattern with no matches is a working object, not a constructor failure. */
$git = new GlobIterator("$giDir/*.nope");
echo "no matches   : count=", count($git), ' valid=', var_export($git->valid(), true),
     ' path=', var_export(giClean($git->getPath()), true), "\n";

/* The prefix is accepted and is what the `glob` debug key shows either way. */
foreach ([ "$giDir/*.txt", "glob://$giDir/*.txt" ] as $giP) {
    $git = new GlobIterator($giP);
    foreach ($git->__debugInfo() as $giK => $giV) {
        echo '  ', str_replace("\0", '|', $giK), ' => ', var_export(giClean($giV), true), "\n";
    }
}
/* Both of php's prefix tests are CASE-SENSITIVE where the wrapper LOOKUP is
 * not, so an upper-case scheme is prefixed AGAIN and then matches nothing. */
$git = new GlobIterator("GLOB://$giDir/*.txt");
echo "upper prefix : count=", count($git);
foreach ($git->__debugInfo() as $giK => $giV) {
    if (strpos($giK, 'glob') !== false) { echo ' glob=', var_export(giClean($giV), true), "\n"; }
}
/* ...and an ordinary directory iterator still shows false there. */
foreach ((new FilesystemIterator($giDir))->__debugInfo() as $giK => $giV) {
    if (strpos($giK, 'glob') !== false) { echo '  plain dir ', str_replace("\0", '|', $giK),
        ' => ', var_export($giV, true), "\n"; }
}

/* The two `check` handlers php gives it. */
try { $git = new GlobIterator("$giDir/*.txt"); $giC = clone $git; echo "clone: ok\n"; }
catch (Throwable $giT) { echo 'clone: ', get_class($giT), ': ', $giT->getMessage(), "\n"; }
class GiSub extends GlobIterator { public function __construct() {} }
$giBad = new GiSub();
foreach (['valid', 'key', 'current', 'rewind', 'getPath', 'getPathname', 'getFilename',
          'getFlags', 'count', 'isDot'] as $giM) {
    try { $giBad->$giM(); echo "  $giM: no refusal\n"; }
    catch (Throwable $giT) { echo "  $giM: ", get_class($giT), ': ', $giT->getMessage(), "\n"; }
}
/* A subclass that DOES call the parent constructor is an ordinary iterator. */
class GiOk extends GlobIterator {}
$giOk = new GiOk("$giDir/*.txt");
echo "subclass     : count=", count($giOk), ' key=', giClean($giOk->key()),
     ' current=', get_class($giOk->current()), "\n";

/* The constructor's two refusals, and the flags that change what current() is. */
try { new GlobIterator(''); } catch (Throwable $giT) { echo 'empty: ', get_class($giT), ': ', $giT->getMessage(), "\n"; }
try { new GlobIterator("$giDir/\0x"); } catch (Throwable $giT) { echo 'nul: ', get_class($giT), ': ', giClean($giT->getMessage()), "\n"; }
$git = new GlobIterator("$giDir/*.txt", FilesystemIterator::CURRENT_AS_SELF);
echo "as self      : ", get_class($git->current()), "\n";
$git = new GlobIterator("$giDir/*.txt", FilesystemIterator::CURRENT_AS_PATHNAME
                                        | FilesystemIterator::KEY_AS_FILENAME);
echo "as pathname  : key=", var_export($git->key(), true), ' current=', var_export(giClean($git->current()), true), "\n";
$git = new GlobIterator("$giDir/*.txt");
$git->setInfoClass('SplFileObject');
echo "info class   : ", get_class($git->current()), "\n";

/* A trailing slash counts its directories and yields none of them: the match is
 * `d/`, whose part after the last slash is EMPTY, and a walk reads that as the
 * end. glob() lists the same directories. */
$git = new GlobIterator("$giDir/*/");
echo "trailing /   : count=", count($git), ' yielded=', json_encode(array_keys(iterator_to_array($git))),
     ' glob=', json_encode(array_map('giClean', glob("$giDir/*/"))), "\n";

foreach (['s1/x.txt', 's2/y.txt', '1.txt', '2.txt', '10.txt', 'z.log', '.hidden'] as $giF) { @unlink("$giDir/$giF"); }
foreach (['s1', 's2'] as $giD) { @rmdir("$giDir/$giD"); }
@rmdir($giDir);
--EXPECT--
class exists : true
parent       : FilesystemIterator
interfaces   : ["Countable","Iterator","SeekableIterator","Stringable","Traversable"]
own methods  : ["__construct","count"]
flags        : 0
count        : 3 3
  <d>/1.txt => SplFileInfo path=<d> file=1.txt name=<d>/1.txt
  <d>/10.txt => SplFileInfo path=<d> file=10.txt name=<d>/10.txt
  <d>/2.txt => SplFileInfo path=<d> file=2.txt name=<d>/2.txt
past end     : false key='' path='' name=''
  walk path=<d>/s1 file=x.txt
  walk path=<d>/s2 file=y.txt
dots         : ["<d>\/.","<d>\/..","<d>\/.hidden"]
dots skipped : ["<d>\/.hidden"] count=3
no matches   : count=0 valid=false path=''
  |SplFileInfo|pathName => '<d>/1.txt'
  |SplFileInfo|fileName => '1.txt'
  |DirectoryIterator|glob => 'glob://<d>/*.txt'
  |RecursiveDirectoryIterator|subPathName => ''
  |SplFileInfo|pathName => '<d>/1.txt'
  |SplFileInfo|fileName => '1.txt'
  |DirectoryIterator|glob => 'glob://<d>/*.txt'
  |RecursiveDirectoryIterator|subPathName => ''
upper prefix : count=0 glob='glob://GLOB://<d>/*.txt'
  plain dir |DirectoryIterator|glob => false
clone: Error: Trying to clone an uncloneable object of class GlobIterator
  valid: Error: The parent constructor was not called: the object is in an invalid state
  key: Error: The parent constructor was not called: the object is in an invalid state
  current: Error: The parent constructor was not called: the object is in an invalid state
  rewind: Error: The parent constructor was not called: the object is in an invalid state
  getPath: Error: The parent constructor was not called: the object is in an invalid state
  getPathname: Error: The parent constructor was not called: the object is in an invalid state
  getFilename: Error: The parent constructor was not called: the object is in an invalid state
  getFlags: Error: The parent constructor was not called: the object is in an invalid state
  count: Error: The parent constructor was not called: the object is in an invalid state
  isDot: Error: The parent constructor was not called: the object is in an invalid state
subclass     : count=3 key=<d>/1.txt current=SplFileInfo
empty: ValueError: GlobIterator::__construct(): Argument #1 ($pattern) must not be empty
nul: ValueError: GlobIterator::__construct(): Argument #1 ($pattern) must not contain any null bytes
as self      : GlobIterator
as pathname  : key='1.txt' current='<d>/1.txt'
info class   : SplFileObject
trailing /   : count=2 yielded=[] glob=["<d>\/s1\/","<d>\/s2\/"]
