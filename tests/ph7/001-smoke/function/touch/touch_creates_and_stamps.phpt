--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
touch() creates a missing file, defaults both stamps to now, and honours $atime
--SKIPIF--
skip: flaky
--FILE--
<?php
$tcs_dir = sys_get_temp_dir() . '/phl_touch_' . getmypid();
mkdir($tcs_dir);
$tcs_f = $tcs_dir . '/new.txt';

// the main use of the idiom: create an empty file. This only ever called utime(),
// which fails with ENOENT, so it answered false and created nothing.
var_dump(touch($tcs_f), file_exists($tcs_f), filesize($tcs_f));
// both stamps default to NOW
var_dump(abs(filemtime($tcs_f) - time()) < 30, abs(fileatime($tcs_f) - time()) < 30);
// $mtime alone drives both
var_dump(touch($tcs_f, 1000000000), filemtime($tcs_f), fileatime($tcs_f));
// $atime is its own argument -- it used to be read off $mtime, so it was ignored
var_dump(touch($tcs_f, 1200000000, 1300000000), filemtime($tcs_f), fileatime($tcs_f));
// a NEGATIVE stamp is a legal timestamp, not a "not given" sentinel
var_dump(touch($tcs_f, -100), filemtime($tcs_f), fileatime($tcs_f));
var_dump(touch($tcs_f, -1), filemtime($tcs_f));
var_dump(touch($tcs_f, 0), filemtime($tcs_f));
// null means "use the default", and $atime alone cannot be given
var_dump(touch($tcs_f, null), abs(filemtime($tcs_f) - time()) < 30);
var_dump(touch($tcs_f, 123, null), filemtime($tcs_f), fileatime($tcs_f));
try {
    touch($tcs_f, null, 1400000000);
    echo "NO_THROW\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// a directory can be stamped too
var_dump(touch($tcs_dir, 1500000000), filemtime($tcs_dir));
// an unwritable directory still fails
var_dump(@touch($tcs_dir . '/nodir/x.txt'));

unlink($tcs_f);
rmdir($tcs_dir);
?>
--EXPECT--
bool(true)
bool(true)
int(0)
bool(true)
bool(true)
bool(true)
int(1000000000)
int(1000000000)
bool(true)
int(1200000000)
int(1300000000)
bool(true)
int(-100)
int(-100)
bool(true)
int(-1)
bool(true)
int(0)
bool(true)
bool(true)
bool(true)
int(123)
int(123)
touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) is an integer
bool(true)
int(1500000000)
bool(false)
--CLEAN--
<?php
