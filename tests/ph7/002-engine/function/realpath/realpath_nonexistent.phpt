--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
realpath() returns FALSE for non-existent path
--SKIPIF--
<?php
if (PHP_OS == 'WINNT') {
    echo "skip";
}
?>
--FILE--
<?php
$path = sys_get_temp_dir() . DIRECTORY_SEPARATOR . "ph7_realpath_nonexistent_" . uniqid();
// Ensure doesn't exist
@unlink($path);
echo (realpath($path) === false ? 'false' : 'true') . "\n";
?>
--EXPECT--
false

