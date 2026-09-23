--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dir() answers php's Directory: no constructor, readonly slots, closed is fatal
--DESCRIPTION--
php's Directory is FINAL and has no constructor at all — `dir()` builds it — so
`new Directory` is refused in the create_object handler with a sentence that
names dir() as the way to get one. Its two slots are
`public protected(set) readonly`, and a method called after `close()` is a
TypeError naming the CLASS, not a warning. The embedded PHP had a public
constructor, a `__destruct` php does not declare, untyped writable slots, and it
answered a Directory whose `handle` was `false` where php answers `false`
outright.
--FILE--
<?php
function dirShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$dirBase = __DIR__ . '/dir_directory_native_tmp';
@mkdir($dirBase);
file_put_contents($dirBase . '/a.txt', 'a');
file_put_contents($dirBase . '/b.txt', 'b');

echo "-- dir() opens, and the object is php's shape\n";
$d = dir($dirBase);
dirShow('class', fn() => get_class($d));
dirShow('path', fn() => basename($d->path));
dirShow('handle is a resource', fn() => gettype($d->handle));
$entries = [];
while (false !== ($e = $d->read())) { $entries[] = $e; }
sort($entries);
dirShow('entries', fn() => implode(',', $entries));
$d->rewind();
dirShow('rewind restarts', fn() => $d->read() !== false);

echo "-- the slots are readonly\n";
dirShow('write path', function () use ($d) { $d->path = 'x'; return 'written'; });
dirShow('write handle', function () use ($d) { $d->handle = 1; return 'written'; });
dirShow('path modifiers', fn() => implode(' ', Reflection::getModifierNames(
    (new ReflectionProperty('Directory', 'path'))->getModifiers())));

echo "-- a closed handle is a TypeError, not a warning\n";
$d->close();
dirShow('read after close', fn() => $d->read());
dirShow('rewind after close', fn() => $d->rewind());
dirShow('close after close', fn() => $d->close());

echo "-- the declaration\n";
dirShow('final', fn() => (new ReflectionClass('Directory'))->isFinal());
dirShow('methods', fn() => implode(',', get_class_methods('Directory')));
dirShow('new Directory', fn() => new Directory);
dirShow('newInstance', fn() => (new ReflectionClass('Directory'))->newInstance());
dirShow('read return type', fn() => (string)(new ReflectionMethod('Directory', 'read'))->getReturnType());

echo "-- dir() on a missing path is false, with php's warning\n";
dirShow('missing', fn() => @dir($dirBase . '/nope'));
dirShow('arity', fn() => dir());

@unlink($dirBase . '/a.txt');
@unlink($dirBase . '/b.txt');
@rmdir($dirBase);
--EXPECT--
-- dir() opens, and the object is php's shape
class => 'Directory'
path => 'dir_directory_native_tmp'
handle is a resource => 'resource'
entries => '.,..,a.txt,b.txt'
rewind restarts => true
-- the slots are readonly
write path => Error: Cannot modify readonly property Directory::$path
write handle => Error: Cannot modify readonly property Directory::$handle
path modifiers => 'public protected(set) readonly'
-- a closed handle is a TypeError, not a warning
read after close => TypeError: Directory::read(): cannot use Directory resource after it has been closed
rewind after close => TypeError: Directory::rewind(): cannot use Directory resource after it has been closed
close after close => TypeError: Directory::close(): cannot use Directory resource after it has been closed
-- the declaration
final => true
methods => 'close,rewind,read'
new Directory => Error: Cannot directly construct Directory, use dir() instead
newInstance => Error: Cannot directly construct Directory, use dir() instead
read return type => 'string|false'
-- dir() on a missing path is false, with php's warning
missing => false
arity => ArgumentCountError: dir() expects at least 1 argument, 0 given
