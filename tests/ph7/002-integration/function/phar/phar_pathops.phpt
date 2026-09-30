--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/phar: the four path operations a phar:// url answers
--DESCRIPTION--
php gives its phar wrapper unlink(), rename(), mkdir() and rmdir(), and each is
its own door: they do not share a refusal, they do not share the `phar.readonly`
rule (a data archive's unlink is allowed and its mkdir is not), and only rename
is told about a second path -- which has to name the SAME archive.

chmod() and touch() are here because they are the two that are NOT the wrapper's:
php's phar implements no stream_metadata, so chmod() takes the engine's
non-standard-stream sentence, while touch() is answered by opening the entry.
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
$dir = sys_get_temp_dir() . '/phlt-pathop-' . getmypid();
@mkdir($dir);
foreach (glob($dir . '/*') as $f) { @unlink($f); }
set_error_handler(function ($n, $s) use ($dir) {
    if (!(error_reporting() & $n)) { return true; }
    echo '    [', $n, '] ', str_replace([$dir, strtr($dir, '/', '\\')], '<d>', $s), "\n";
    return true;
});
foreach (['a.phar' => 'Phar', 'b.tar' => 'PharData'] as $name => $class) {
    $o = new $class("$dir/$name");
    $o->addFromString('x.txt', 'X');
    $o->addFromString('keep.txt', 'K');
    $o->addFromString('sub/y.txt', 'Y');
    unset($o);
}
echo "-- every door, on an archive php lets a script write\n";
foreach (['a.phar', 'b.tar'] as $name) {
    $a = "$dir/$name";
    echo "-- $name\n";
    echo "  unlink a file    : "; var_dump(unlink("phar://$a/x.txt"));
    echo "  unlink a missing : "; var_dump(unlink("phar://$a/nope.txt"));
    echo "  unlink a dir     : "; var_dump(unlink("phar://$a/sub"));
    echo "  rmdir non-empty  : "; var_dump(rmdir("phar://$a/sub"));
    echo "  rmdir a missing  : "; var_dump(rmdir("phar://$a/nodir"));
    echo "  rmdir a file     : "; var_dump(rmdir("phar://$a/sub/y.txt"));
    echo "  mkdir            : "; var_dump(mkdir("phar://$a/nd"));
    echo "  ...is a dir      : "; var_dump(is_dir("phar://$a/nd"));
    echo "  ...and rmdir it  : "; var_dump(rmdir("phar://$a/nd"));
    echo "  mkdir an existing: "; var_dump(mkdir("phar://$a/sub"));
    echo "  mkdir over a file: "; var_dump(mkdir("phar://$a/sub/y.txt"));
    echo "  rename a file    : "; var_dump(rename("phar://$a/sub/y.txt", "phar://$a/sub/z.txt"));
    echo "  ...and read it   : "; var_dump(@file_get_contents("phar://$a/sub/z.txt"));
    echo "  rename a missing : "; var_dump(rename("phar://$a/gone", "phar://$a/g2"));
    echo "  rename a DIR     : "; var_dump(rename("phar://$a/sub", "phar://$a/sub2"));
    echo "  ...and read that : "; var_dump(@file_get_contents("phar://$a/sub2/z.txt"));
    echo "  chmod            : "; var_dump(chmod("phar://$a/keep.txt", 0644));
    echo "  touch an entry   : "; var_dump(touch("phar://$a/keep.txt"));
    echo "  touch a missing  : "; var_dump(touch("phar://$a/none.txt"));
}
echo "-- and the two ends of a rename are ONE archive's\n";
echo "  a data archive   : "; var_dump(rename("phar://$dir/a.phar/keep.txt", "phar://$dir/b.tar/keep.txt"));
echo "  one not there    : "; var_dump(rename("phar://$dir/a.phar/keep.txt", "phar://$dir/new.phar/keep.txt"));
echo "  a plain path     : "; var_dump(rename("phar://$dir/a.phar/keep.txt", "$dir/out.txt"));
echo "-- a rename REPLACES what the destination held\n";
$q = "$dir/c.phar";
$o = new Phar($q);
$o->addFromString('one.txt', 'ONE');
$o->addFromString('two.txt', 'TWO');
unset($o);
var_dump(rename("phar://$q/one.txt", "phar://$q/two.txt"));
var_dump(file_get_contents("phar://$q/two.txt"), file_exists("phar://$q/one.txt"));

foreach (glob($dir . '/*') as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- every door, on an archive php lets a script write
-- a.phar
  unlink a file    : bool(true)
  unlink a missing :     [2] unlink(): unlink of "phar://<d>/a.phar/nope.txt" failed, file does not exist
bool(false)
  unlink a dir     :     [2] unlink(): unlink of "phar://<d>/a.phar/sub" failed, file does not exist
bool(false)
  rmdir non-empty  :     [2] rmdir(): phar error: Directory not empty
bool(false)
  rmdir a missing  :     [2] rmdir(): phar error: cannot remove directory "nodir" in phar "<d>/a.phar", directory does not exist
bool(false)
  rmdir a file     :     [2] rmdir(): phar error: cannot remove directory "sub/y.txt" in phar "<d>/a.phar", phar error: path "sub/y.txt" exists and is a not a directory
bool(false)
  mkdir            : bool(true)
  ...is a dir      : bool(true)
  ...and rmdir it  : bool(true)
  mkdir an existing:     [2] mkdir(): phar error: cannot create directory "sub" in phar "<d>/a.phar", directory already exists
bool(false)
  mkdir over a file:     [2] mkdir(): phar error: cannot create directory "sub/y.txt" in phar "<d>/a.phar", phar error: path "sub/y.txt" exists and is a not a directory
bool(false)
  rename a file    : bool(true)
  ...and read it   : string(1) "Y"
  rename a missing :     [2] rename(): phar error: cannot rename "phar://<d>/a.phar/gone" to "phar://<d>/a.phar/g2" from extracted phar archive, source does not exist
bool(false)
  rename a DIR     : bool(true)
  ...and read that : string(1) "Y"
  chmod            :     [2] chmod(): Cannot call chmod() for a non-standard stream
bool(false)
  touch an entry   : bool(true)
  touch a missing  :     [2] touch(phar://<d>/a.phar/none.txt): Failed to open stream: phar error: "none.txt" is not a file in phar "<d>/a.phar"
bool(false)
-- b.tar
  unlink a file    : bool(true)
  unlink a missing :     [2] unlink(): unlink of "phar://<d>/b.tar/nope.txt" failed, file does not exist
bool(false)
  unlink a dir     :     [2] unlink(): unlink of "phar://<d>/b.tar/sub" failed, file does not exist
bool(false)
  rmdir non-empty  :     [2] rmdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  rmdir a missing  :     [2] rmdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  rmdir a file     :     [2] rmdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  mkdir            :     [2] mkdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  ...is a dir      : bool(false)
  ...and rmdir it  :     [2] rmdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  mkdir an existing:     [2] mkdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  mkdir over a file:     [2] mkdir(): Cannot create phar '<d>/b.tar', file extension (or combination) not recognised or the directory does not exist
bool(false)
  rename a file    :     [2] rename(): phar error: cannot rename "phar://<d>/b.tar/sub/y.txt" to "phar://<d>/b.tar/sub/z.txt": invalid or non-writable url "phar://<d>/b.tar/sub/y.txt"
bool(false)
  ...and read it   : bool(false)
  rename a missing :     [2] rename(): phar error: cannot rename "phar://<d>/b.tar/gone" to "phar://<d>/b.tar/g2": invalid or non-writable url "phar://<d>/b.tar/gone"
bool(false)
  rename a DIR     :     [2] rename(): phar error: cannot rename "phar://<d>/b.tar/sub" to "phar://<d>/b.tar/sub2": invalid or non-writable url "phar://<d>/b.tar/sub"
bool(false)
  ...and read that : bool(false)
  chmod            :     [2] chmod(): Cannot call chmod() for a non-standard stream
bool(false)
  touch an entry   : bool(true)
  touch a missing  :     [2] touch(phar://<d>/b.tar/none.txt): Failed to open stream: phar error: "none.txt" is not a file in phar "<d>/b.tar"
bool(false)
-- and the two ends of a rename are ONE archive's
  a data archive   :     [2] rename(): phar error: cannot rename "phar://<d>/a.phar/keep.txt" to "phar://<d>/b.tar/keep.txt": invalid or non-writable url "phar://<d>/b.tar/keep.txt"
bool(false)
  one not there    :     [2] rename(): phar error: cannot rename "phar://<d>/a.phar/keep.txt" to "phar://<d>/new.phar/keep.txt", not within the same phar archive
bool(false)
  a plain path     :     [2] rename(): Cannot rename a file across wrapper types
bool(false)
-- a rename REPLACES what the destination held
bool(true)
string(3) "ONE"
bool(false)
