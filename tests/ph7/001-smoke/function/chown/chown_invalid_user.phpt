--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (PHP_OS === 'WINNT') {
    echo "skip POSIX only";
}
?>
--TEST--
chown() with an unknown name warns and returns false
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$chownInvTmp = tempnam(sys_get_temp_dir(), 'phl_');
file_put_contents($chownInvTmp, 'test');
echo var_export(chown($chownInvTmp, 'nonexistentuser12345'), true), "\n";
unlink($chownInvTmp);
restore_error_handler();
?>
--EXPECT--
[2] chown(): Unable to find uid for nonexistentuser12345
false
--CLEAN--
<?php
