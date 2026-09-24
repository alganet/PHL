--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session.serialize_handler naming a handler that does not exist — php.ini arms it, session_start() refuses
--DESCRIPTION--
ini_set() refuses an unregistered serializer outright, so only php.ini / -d can
leave one armed. php reports it where the session would have started and starts
nothing at all.
--INI--
session.serialize_handler=bogus
--FILE--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessbog_" . getmypid();
@mkdir($dir);
session_save_path($dir);
session_id("bog" . getmypid());
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) {
    $warn[] = $msg;
    return true;
});
$r = session_start();
$_SESSION["a"] = 1;
$w = session_write_close();
restore_error_handler();
echo "handler: ", ini_get("session.serialize_handler"), "\n";
echo "start:   ", var_export($r, true), "\n";
echo "close:   ", var_export($w, true), "\n";
echo "status:  ", session_status(), "\n";
echo "file:    ", var_export(@file_get_contents($dir . "/sess_bog" . getmypid()), true), "\n";
echo implode("\n", $warn), "\n";
?>
--EXPECT--
handler: bogus
start:   false
close:   false
status:  0
file:    false
session_start(): Cannot find session serialization handler "bogus" - session startup failed
--CLEAN--
<?php
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsessbog_" . getmypid();
foreach (glob($dir . "/sess_*") as $f) { @unlink($f); }
@rmdir($dir);
