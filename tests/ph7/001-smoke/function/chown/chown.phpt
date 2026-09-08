--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
// Root CAN reassign ownership, so the failure this test pins would not happen.
// The handler swallows the probe's own warning: a SKIPIF that prints anything is
// read as "skip <that text>".
set_error_handler(function () { return true; });
$skipTmp = tempnam(sys_get_temp_dir(), 'skip_');
$skipOk = chown($skipTmp, 0);
unlink($skipTmp);
restore_error_handler();
if (PHP_OS === 'WINNT' || $skipOk) {
    echo "skip needs a non-root POSIX host";
}
?>
--TEST--
chown() on a file we cannot reassign warns and returns false
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$chownTmp = tempnam(sys_get_temp_dir(), 'phl_');
echo var_export(chown($chownTmp, 0), true), "\n";
unlink($chownTmp);
restore_error_handler();
?>
--EXPECT--
[2] chown(): Operation not permitted
false
--CLEAN--
<?php
