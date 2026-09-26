--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A subclass of PDO releases its database the way PDO itself does
--DESCRIPTION--
The native teardown that closes a sqlite handle is resolved through the class's
ANCESTORS, like php's own free_obj handler, so a subclass inherits it. Two
subclasses reach this: `Pdo\Sqlite`, which is what PDO::connect() answers, and
any userland `extends PDO`. Reading the hook off the instance's own class left
both of them holding an open database after their last reference went away.

It is deliberately hard to see from PHP -- that is why php uses a handler here
rather than a __destruct anyone could observe -- so this asserts the one
consequence that IS visible: an open sqlite handle keeps its file open, and
Windows refuses to unlink a file that something still has open. On POSIX the
unlink succeeds either way and only the sanitizer sees the difference.
--FILE--
<?php
$dir = __DIR__ . '/pdo_subclass_release_scratch';
@mkdir($dir);

class PdoSubclassReleaseHandle extends PDO {}

foreach (['userland subclass', 'connect subclass', 'PDO itself'] as $i => $what) {
    $file = $dir . '/r' . $i . '.db';
    @unlink($file);
    switch ($i) {
        case 0: $db = new PdoSubclassReleaseHandle('sqlite:' . $file); break;
        case 1: $db = PDO::connect('sqlite:' . $file); break;
        default: $db = new PDO('sqlite:' . $file); break;
    }
    $db->exec('CREATE TABLE t (a INT)');
    echo $what, ': ', get_class($db), "\n";
    unset($db);
    var_dump(file_exists($file), unlink($file), file_exists($file));
}
?>
--EXPECT--
userland subclass: PdoSubclassReleaseHandle
bool(true)
bool(true)
bool(false)
connect subclass: Pdo\Sqlite
bool(true)
bool(true)
bool(false)
PDO itself: PDO
bool(true)
bool(true)
bool(false)
--CLEAN--
<?php
$dir = __DIR__ . '/pdo_subclass_release_scratch';
foreach (glob($dir . '/*.db') as $f) { @unlink($f); }
@rmdir($dir);
?>
