--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rmdir() should return FALSE for non-existent path
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$path = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'ph7_rmdir_nonexistent_' . uniqid();
echo rmdir($path) ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($path);
