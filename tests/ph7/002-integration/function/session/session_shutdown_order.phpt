--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: an open session is written back AFTER the script's shutdown functions
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessord_" . getmypid();
@mkdir($dir);
session_save_path($dir);
session_id("ord" . getmypid());
session_start();
$_SESSION["early"] = 1;
$file = $dir . "/sess_" . session_id();
// php writes the session from its own module shutdown, which runs after every
// register_shutdown_function() callback: the session is still ACTIVE in here and
// what this writes still reaches the file.
register_shutdown_function(function () {
    echo "status-in-shutdown: ", session_status(), "\n";
    $_SESSION["late"] = 2;
});
register_shutdown_function(function () use ($file) {
    echo "write-close: ", var_export(session_write_close(), true), "\n";
    echo "file: ", file_get_contents($file), "\n";
});
echo "body\n";
?>
--EXPECT--
body
status-in-shutdown: 2
write-close: true
file: early|i:1;late|i:2;
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessord_" . getmypid();
@unlink($dir . "/sess_ord" . getmypid());
@rmdir($dir);
