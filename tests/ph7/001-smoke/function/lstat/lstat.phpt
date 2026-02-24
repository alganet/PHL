--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
lstat() should report 0 size for a symlink and stat() should report the file size
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo 'skip: windows';
}
if (function_exists('zend_version')) echo "skip";
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_lstat_');
file_put_contents($fname, 'Hello');
$sym = $fname . '.lnk';
$ok = symlink($fname, $sym);
if ($ok) {
    $st = stat($sym);
    echo "stat_size=" . $st['size'] . PHP_EOL;
    $l = lstat($sym);
    echo "lstat_size=" . $l['size'] . PHP_EOL;
} else {
    echo "symlink_failed\n";
}
?>
--EXPECTF--
stat_size=5
lstat_size=%d
--CLEAN--
<?php
@unlink($sym);
@unlink($fname);
unset($tmp, $ln, $fname, $sym, $ok, $st, $l);
