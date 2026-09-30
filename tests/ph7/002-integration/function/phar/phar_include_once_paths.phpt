--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/phar: two spellings of one entry are ONE included file
--DESCRIPTION--
`require_once` keys its registry on the file's canonical name. A path on the
filesystem gets that from realpath(); a `phar://` url has no realpath() to ask,
so `phar://x.phar/bin/../inc.php` and `phar://x.phar/bin/../vendor/../inc.php`
were two rows and the second one re-included the file --
`Cannot redeclare interface I`. It is exactly the shape phpstan.phar reaches its
own src/Analyser/InternalScopeFactory.php by from preload.php, so no phpstan run
started. The archive's reader already collapsed `.` and `..` when it looked an
ENTRY up; what it did not do was answer under the collapsed name.
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
$dir = sys_get_temp_dir() . '/phlt-phar-inc-' . getmypid();
@mkdir($dir);
foreach (glob($dir . '/*') as $f) { @unlink($f); }
$p = $dir . '/inc.phar';

$ph = new Phar($p);
$ph->addFromString('inc.php',
    "<?php\n"
    . "\$dd = [realpath(\$GLOBALS['d']) ?: \$GLOBALS['d'], \$GLOBALS['d']];\n"
    . "echo 'FILE=', str_replace(\$dd, '<d>', __FILE__), \"\\n\";\n"
    . "echo 'DIR=', str_replace(\$dd, '<d>', __DIR__), \"\\n\";\n"
    . "interface PharIncOnce {}\n");
$ph->addFromString('vendor/x.php', "<?php\n");
$ph->addFromString('bin/run.php',
    "<?php\n"
    . "require_once __DIR__ . '/../inc.php';\n"
    . "require_once __DIR__ . '/../vendor/../inc.php';\n"
    . "require_once __DIR__ . '/.././inc.php';\n"
    . "require_once 'phar://' . \$GLOBALS['d'] . '/inc.phar/inc.php';\n"
    . "echo 'run reached the end', \"\\n\";\n");
unset($ph);

$GLOBALS['d'] = $dir;
echo "-- one entry, four spellings\n";
require "phar://$p/bin/run.php";

echo "-- and the entry is still reachable by any of them\n";
var_dump(file_get_contents("phar://$p/bin/../vendor/x.php"));
var_dump(interface_exists('PharIncOnce'));

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- one entry, four spellings
FILE=phar://<d>/inc.phar/inc.php
DIR=phar://<d>/inc.phar
run reached the end
-- and the entry is still reachable by any of them
string(6) "<?php
"
bool(true)
