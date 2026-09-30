--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every SQLite3 filename but two is expanded to an absolute path before sqlite sees it
--DESCRIPTION--
php runs an ext/sqlite3 filename through expand_filepath() unless it is exactly
`:memory:` or the empty string -- the two names that name no file. Everything
else is a path and reaches the library already absolute, so the same relative
name opened from two directories is two databases.

That expansion is not a convenience: it is also what keeps a `file:...` name
from ever being read as a sqlite URI, because by then the name starts with the
working directory. The engine cannot leave that to the library, since a Debian
libsqlite3 is compiled with URI filenames ON and a vcpkg one is not -- the same
string would open two different things on this engine's two platforms. ext/pdo
passes SQLITE_OPEN_URI on purpose and answers otherwise; the divergence between
the two extensions is php's own. (The `file:` name itself is pinned by the
POSIX-only sibling of this test, since a colon cannot be part of a Windows
filename.)

The `.` and `..` segments are collapsed by the expansion rather than by the
filesystem, so `no/such/../../rel3.db` opens `rel3.db` even though no `no`
directory exists -- where handing the OS that path would be ENOENT. A directory
that is genuinely missing after the collapse still fails.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/sq3fn' . getmypid();
@mkdir($dir);
/* This test LISTS its directory, and a pid the OS reuses can land it on one a
 * killed run left behind -- which is a stray name in the listing months later.
 * Start from an empty directory rather than a merely existing one. */
foreach (glob($dir . '/*') as $f) {
    if (is_dir($f)) {
        foreach (glob($f . '/*') as $g) { @unlink($g); }
        @rmdir($f);
    } else {
        @unlink($f);
    }
}
$old = getcwd();
chdir($dir);

$open = function ($name) {
    try { $d = new SQLite3($name); $d->exec('CREATE TABLE t (a)'); $d->close(); return 'ok'; }
    catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
};
foreach ([':memory:', '', 'rel.db', './rel2.db',
          'no/such/../../rel3.db', 'a/b/c.db'] as $name) {
    echo str_pad(var_export($name, true), 24), ' => ', $open($name), "\n";
}
$left = array_values(array_diff(scandir($dir), ['.', '..']));
sort($left);
echo "files: ", implode(' ', $left), "\n";

/* the same name from a DIFFERENT directory is a different database */
@mkdir($dir . '/sub');
chdir($dir . '/sub');
echo 'from a subdir       => ', $open('rel.db'), "\n";
echo 'and it is its own   => ', var_export(file_exists($dir . '/sub/rel.db'), true), "\n";

/* an absolute name is taken as it stands */
echo 'absolute            => ', $open($dir . '/abs.db'), "\n";
echo 'absolute landed     => ', var_export(file_exists($dir . '/abs.db'), true), "\n";

chdir($old);
foreach (['rel.db', 'rel2.db', 'rel3.db',
          'abs.db', 'sub/rel.db'] as $f) { @unlink($dir . '/' . $f); }
@rmdir($dir . '/sub');
@rmdir($dir);
--EXPECT--
':memory:'               => ok
''                       => ok
'rel.db'                 => ok
'./rel2.db'              => ok
'no/such/../../rel3.db'  => ok
'a/b/c.db'               => Exception: Unable to open database: unable to open database file
files: rel.db rel2.db rel3.db
from a subdir       => ok
and it is its own   => true
absolute            => ok
absolute landed     => true
