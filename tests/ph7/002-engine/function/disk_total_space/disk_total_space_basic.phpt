--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
disk_total_space basic test
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' || function_exists('zend_version')) {
    echo "skip: platform";
}
?>
--FILE--
<?php
$space = disk_total_space('/');
echo is_numeric($space) && $space > 0 ? 'OK' : 'FAIL';
?>
--EXPECTF--
%s Warning: disk_total_space(): IO routine(disk_total_space) not implemented in the underlying VFS,PH7 is returning FALSE
FAIL
