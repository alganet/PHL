--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a failed open names the REASON, on every platform
--FILE--
<?php
/* php words an IO failure "Failed to open stream: <the system's reason>", and
 * the reason comes from errno. On Windows the native calls report through
 * GetLastError() and leave errno alone, so PHL's whole file/directory open
 * family said "No error" -- or whatever an unrelated earlier call had left in
 * errno -- for a file that is simply not there. */
set_error_handler(function ($n, $s) { echo "  ERR[$n] $s\n"; return true; });
$of_miss = 'phl_no_such_file_xyz';
$of_dir  = 'phl_no_such_dir_xyz';

var_dump(@file_get_contents($of_miss));
var_dump(@fopen($of_miss, 'r'));
var_dump(@readfile($of_miss));
/* php's plain-files opener on Windows warns with the SYSTEM's reason first --
 * FormatMessage()'s text cut two characters short, php's own slip -- and only
 * then with the errno one every platform shares. */
if (DIRECTORY_SEPARATOR === '/') {
    var_dump(true);
    var_dump(@opendir($of_dir));
} else {
    $of_w = [];
    set_error_handler(function ($n, $s) use (&$of_w) { $of_w[] = $s; return true; });
    $of_r = @opendir($of_dir);
    restore_error_handler();
    var_dump(array_shift($of_w) === "opendir($of_dir): The system cannot find the file specifi (code: 2)");
    foreach ($of_w as $of_s) { echo "  ERR[2] $of_s\n"; }
    var_dump($of_r);
}
var_dump(@md5_file($of_miss));

/* The same question the other way round: a WRITE into a directory that is not
 * there is ENOENT too, not a silent false. */
var_dump(@file_put_contents($of_dir . '/x', 'y'));
?>
--EXPECT--
  ERR[2] file_get_contents(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
  ERR[2] fopen(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
  ERR[2] readfile(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
bool(true)
  ERR[2] opendir(phl_no_such_dir_xyz): Failed to open directory: No such file or directory
bool(false)
  ERR[2] md5_file(phl_no_such_file_xyz): Failed to open stream: No such file or directory
bool(false)
  ERR[2] file_put_contents(phl_no_such_dir_xyz/x): Failed to open stream: No such file or directory
bool(false)
--CLEAN--
<?php
unset($of_miss, $of_dir);
