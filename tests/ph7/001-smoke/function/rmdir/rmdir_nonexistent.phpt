--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rmdir() on a missing directory warns and returns false
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    echo "[$no] ", preg_replace('#\(.*\)#', '(PATH)', $msg), "\n";
    return true;
});
$rmdirPath = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phl_rmdir_nonexistent_' . uniqid();
echo rmdir($rmdirPath) ? "true\n" : "false\n";
restore_error_handler();
?>
--EXPECT--
[2] rmdir(PATH): No such file or directory
false
--CLEAN--
<?php
unset($rmdirPath);
