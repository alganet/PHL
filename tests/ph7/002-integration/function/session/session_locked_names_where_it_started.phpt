--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a session refusal names the session_start behind it
--FILE--
<?php
$sessDir = rtrim(sys_get_temp_dir(), "/") . "/phlwhere_" . getmypid();
@mkdir($sessDir);
session_save_path($sessDir);
session_id("where" . getmypid());
$sessLog = [];
set_error_handler(function ($n, $s) use (&$sessLog) { $sessLog[] = "E$n: $s"; return true; });
session_start();
session_name('x');
session_id('y');
session_save_path('/tmp');
ini_set('session.name', 'z');
ini_restore('session.name');
session_write_close();
restore_error_handler();
foreach ($sessLog as $sessLine) { echo str_replace(__FILE__, 'FILE', $sessLine), "\n"; }
?>
--EXPECT--
E2: session_name(): Session name cannot be changed when a session is active (started from FILE on line 8)
E2: session_id(): Session ID cannot be changed when a session is active (started from FILE on line 8)
E2: session_save_path(): Session save path cannot be changed when a session is active (started from FILE on line 8)
E2: ini_set(): Session ini settings cannot be changed when a session is active (started from FILE on line 8)
E2: ini_restore(): Session ini settings cannot be changed when a session is active (started from FILE on line 8)
--CLEAN--
<?php
$sessDir = rtrim(sys_get_temp_dir(), "/") . "/phlwhere_" . getmypid();
foreach (glob($sessDir . "/sess_*") as $f) { @unlink($f); }
@rmdir($sessDir);
