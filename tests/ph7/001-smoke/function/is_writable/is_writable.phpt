--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_writable() should return true for a writable file
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo "skip: platform";
}
?>
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_writable_');
file_put_contents($fname, 'A');
// Ensure file is writable
@chmod($fname, 0666);
echo "is_writable=" . (is_writable($fname) ? 'true' : 'false') . PHP_EOL;
@unlink($fname);
?>
--EXPECT--
is_writable=true
--CLEAN--
<?php
@unlink($fname);
unset($fname);
