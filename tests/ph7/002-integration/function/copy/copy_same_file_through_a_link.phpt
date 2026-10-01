--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy() refuses a destination that is a hard or symbolic link to its source
--DESCRIPTION--
The identity copy() refuses is the one stat() reports, so a second name for the
source -- a hard link, or a symlink resolving to it -- is refused as flatly as
repeating the path, and the source keeps its contents.
--SKIPIF--
<?php
if (stripos(PHP_OS, 'WIN') === 0) {
    echo "skip POSIX link semantics";
}
?>
--FILE--
<?php
$d = sys_get_temp_dir() . "/copylink" . getmypid();
@mkdir($d);
file_put_contents("$d/a.txt", "hello world");

link("$d/a.txt", "$d/hard.txt");
var_dump(copy("$d/a.txt", "$d/hard.txt"));
var_dump(file_get_contents("$d/hard.txt"));

symlink("$d/a.txt", "$d/soft.txt");
var_dump(copy("$d/a.txt", "$d/soft.txt"));
var_dump(file_get_contents("$d/a.txt"));

// A symlink to a DIFFERENT file is a different file, and copies.
file_put_contents("$d/b.txt", "second");
symlink("$d/b.txt", "$d/other.txt");
var_dump(copy("$d/a.txt", "$d/other.txt"));
var_dump(file_get_contents("$d/b.txt"));

foreach (glob("$d/*") as $f) { @unlink($f); }
@rmdir($d);
?>
--EXPECT--
bool(false)
string(11) "hello world"
bool(false)
string(11) "hello world"
bool(true)
string(11) "hello world"
