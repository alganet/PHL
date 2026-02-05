--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chgrp() should return FALSE when group does not exist
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
// Try to chgrp to non-existent group
$result = chgrp($temp, 'nonexistentgroup12345');
// Clean up
unlink($temp);
echo $result ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($temp, $result);
