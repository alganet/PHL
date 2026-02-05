--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_executable should detect executable bit on unix
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo "skip: platform";
}
?>
--FILE--
<?php
$fname = tempnam(sys_get_temp_dir(), 'ph7_exec_');
file_put_contents($fname, "#!/bin/sh\necho ok\n");
@chmod($fname, 0755);
echo "is_executable=" . (is_executable($fname) ? 'true' : 'false') . PHP_EOL;
@unlink($fname);
?>
--EXPECTF--
is_executable=%s
--CLEAN--
<?php
unset($fname);
