--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
disk_total_space / disk_free_space basic test
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo "skip: the Windows VFS still leaves xFreeSpace/xTotalSpace unimplemented";
}
?>
--FILE--
<?php
$space = disk_total_space('/');
echo is_numeric($space) && $space > 0 ? 'OK' : 'FAIL', "\n";
$free = disk_free_space('/');
echo is_numeric($free) && $free > 0 ? 'OK' : 'FAIL', "\n";
// a filesystem's total capacity can never be smaller than its free space
echo $space >= $free ? 'sane' : 'insane', "\n";
?>
--EXPECT--
OK
OK
sane
--CLEAN--
<?php
unset($space, $free);
