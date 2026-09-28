--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What an EMPTY path is, at every door that takes one
--DESCRIPTION--
`''` is not a path php ever tries, and what it does instead of trying is three
different things depending on which door was asked.

**Every OPENER refuses it** with php's stream-layer `ValueError: Path must not
be empty` -- unqualified by a function name, because the refusal is the layer's
and not the function's -- and that is a catchable exception where this engine
used to warn and answer false. `include`/`require` are openers too.

**Three doors name their own argument instead**: `parse_ini_file()`,
`scandir()`, and the `DirectoryIterator`/`DOM` constructors that already did.

**The STAT family says NOTHING**: an empty path answers the same FALSE a
missing one does and skips the `stat failed for` warning, and `opendir()` and
`disk_free_space()` are silent the same way.

And two doors read an empty path as a REQUEST rather than a refusal:
`realpath('')` resolves `.` and answers the working directory, and `tempnam('')`
takes it as "the system temporary directory" and says nothing -- where a
directory it cannot use gets the file there too, with php's notice.
--SKIPIF--
<?php
if (stripos(PHP_OS, 'WIN') === 0) {
    echo "skip POSIX path semantics";
}
?>
--FILE--
<?php
function why(callable $fn)
{
    $seen = array();
    set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
    try {
        $r = $fn();
        $show = is_resource($r) ? 'resource' : (is_array($r) ? 'array(' . count($r) . ')' : var_export($r, true));
        if (is_resource($r)) { @fclose($r); }
        $out = sprintf("-> %-14s %s", $show, json_encode($seen));
    } catch (\Throwable $t) {
        $out = sprintf("!! %s: %s", get_class($t), $t->getMessage());
    }
    restore_error_handler();
    return $out;
}

$openers = array(
    'file_get_contents' => fn() => file_get_contents(''),
    'file_put_contents' => fn() => file_put_contents('', 'x'),
    'fopen'             => fn() => fopen('', 'r'),
    'file'              => fn() => file(''),
    'readfile'          => fn() => readfile(''),
    'copy source'       => fn() => copy('', sys_get_temp_dir() . '/phlcp'),
    'md5_file'          => fn() => md5_file(''),
    'sha1_file'         => fn() => sha1_file(''),
    'hash_file'         => fn() => hash_file('md5', ''),
    'hash_hmac_file'    => fn() => hash_hmac_file('md5', '', 'k'),
    'getimagesize'      => fn() => getimagesize(''),
    'include'           => fn() => include '',
    'require'           => fn() => require '',
    'SplFileObject'     => fn() => new SplFileObject(''),
);
foreach ($openers as $name => $fn) {
    printf("%-20s %s\n", $name, why($fn));
}

/* copy()'s DESTINATION is refused the same way, with the source already open. */
$src = tempnam(sys_get_temp_dir(), 'ep');
file_put_contents($src, 'x');
printf("%-20s %s\n", 'copy destination', why(fn() => copy($src, '')));
unlink($src);

echo "-- named by their own argument\n";
printf("%-20s %s\n", 'parse_ini_file', why(fn() => parse_ini_file('')));
printf("%-20s %s\n", 'scandir', why(fn() => scandir('')));
printf("%-20s %s\n", 'DirectoryIterator', why(fn() => new DirectoryIterator('')));

echo "-- silent\n";
foreach (array(
    'opendir'         => fn() => opendir(''),
    'filesize'        => fn() => filesize(''),
    'stat'            => fn() => stat(''),
    'lstat'           => fn() => lstat(''),
    'fileatime'       => fn() => fileatime(''),
    'filetype'        => fn() => filetype(''),
    'fileperms'       => fn() => fileperms(''),
    'is_file'         => fn() => is_file(''),
    'file_exists'     => fn() => file_exists(''),
    'disk_free_space' => fn() => disk_free_space(''),
) as $name => $fn) {
    printf("%-20s %s\n", $name, why($fn));
}

echo "-- read as a request\n";
printf("%-20s %s\n", 'realpath', var_export(realpath('') === realpath('.'), true));
/* php names the file under the temp dir's REAL path, which is not the spelling
 * sys_get_temp_dir() gives where that is a symlink (macOS's /var) */
$inTmp = fn($p) => is_string($p)
    && (strpos($p, sys_get_temp_dir()) === 0 || strpos($p, realpath(sys_get_temp_dir())) === 0);
$t = tempnam('', 'ep');
printf("%-20s %s\n", 'tempnam', var_export($inTmp($t), true));
if (is_string($t)) { unlink($t); }
$seen = array();
set_error_handler(function ($no, $str) use (&$seen) { $seen[] = $str; return true; });
$u = tempnam('/nope/zz', 'ep');
restore_error_handler();
printf("%-20s %s / %s\n", 'tempnam elsewhere',
    var_export($inTmp($u), true), json_encode($seen));
if (is_string($u)) { unlink($u); }
?>
--EXPECT--
file_get_contents    !! ValueError: Path must not be empty
file_put_contents    !! ValueError: Path must not be empty
fopen                !! ValueError: Path must not be empty
file                 !! ValueError: Path must not be empty
readfile             !! ValueError: Path must not be empty
copy source          !! ValueError: Path must not be empty
md5_file             !! ValueError: Path must not be empty
sha1_file            !! ValueError: Path must not be empty
hash_file            !! ValueError: Path must not be empty
hash_hmac_file       !! ValueError: Path must not be empty
getimagesize         !! ValueError: Path must not be empty
include              !! ValueError: Path must not be empty
require              !! ValueError: Path must not be empty
SplFileObject        !! ValueError: Path must not be empty
copy destination     !! ValueError: Path must not be empty
-- named by their own argument
parse_ini_file       !! ValueError: parse_ini_file(): Argument #1 ($filename) must not be empty
scandir              !! ValueError: scandir(): Argument #1 ($directory) must not be empty
DirectoryIterator    !! ValueError: DirectoryIterator::__construct(): Argument #1 ($directory) must not be empty
-- silent
opendir              -> false          []
filesize             -> false          []
stat                 -> false          []
lstat                -> false          []
fileatime            -> false          []
filetype             -> false          []
fileperms            -> false          []
is_file              -> false          []
file_exists          -> false          []
disk_free_space      -> false          []
-- read as a request
realpath             true
tempnam              true
tempnam elsewhere    true / ["tempnam(): file created in the system's temporary directory"]
