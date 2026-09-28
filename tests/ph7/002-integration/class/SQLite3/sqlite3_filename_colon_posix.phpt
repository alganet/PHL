--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
/* A colon cannot stand inside a Windows filename: two of these names are
   refused outright there and `file:rel4.db` names an alternate DATA STREAM of a
   file called `file`, which no directory listing shows. */
if (PHP_OS_FAMILY === "Windows") {
    echo "skip needs a POSIX filesystem";
}
?>
--TEST--
A SQLite3 filename that merely resembles :memory: is a file like any other
--DESCRIPTION--
php compares the whole of `:memory:` and nothing shorter, and the comparison is
case-sensitive, so `:memory:extra`, `x:memory:` and `:MEMORY:` are all ordinary
filenames -- each one ends up on disk. `file:rel4.db` is here for the reason its
sibling test explains: the expansion in front of it is what stops sqlite reading
it as a URI, and the proof is a file of that exact name in the directory
afterwards.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/sq3cn' . getmypid();
@mkdir($dir);
$old = getcwd();
chdir($dir);

/* php compares the whole of `:memory:` and nothing shorter, so every other
   spelling is a FILENAME -- one a POSIX filesystem is happy to create. The
   `file:` form is here for the same reason: what keeps it from reaching sqlite
   as a URI is the expansion in front of it, and the proof is a file of that
   name sitting in the directory afterwards. */
foreach ([':memory:extra', 'x:memory:', ':MEMORY:', 'file:rel4.db'] as $name) {
    try { $d = new SQLite3($name); $d->exec('CREATE TABLE t (a)'); $d->close(); $r = 'ok'; }
    catch (Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad(var_export($name, true), 18), ' => ', $r, "\n";
}
$left = array_values(array_diff(scandir($dir), ['.', '..']));
sort($left);
echo "files: ", implode(' ', $left), "\n";

chdir($old);
foreach ([':memory:extra', 'x:memory:', ':MEMORY:', 'file:rel4.db'] as $f) { @unlink($dir . '/' . $f); }
@rmdir($dir);
--EXPECT--
':memory:extra'    => ok
'x:memory:'        => ok
':MEMORY:'         => ok
'file:rel4.db'     => ok
files: :MEMORY: :memory:extra file:rel4.db x:memory:
