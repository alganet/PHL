--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chown() should return FALSE when user does not exist
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' || function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Create a temp file
$temp = tempnam(sys_get_temp_dir(), 'ph7_test');
file_put_contents($temp, 'test');
// Try to chown to non-existent user
$result = chown($temp, 'nonexistentuser12345');
// Clean up
unlink($temp);
echo $result ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($temp, $result);
