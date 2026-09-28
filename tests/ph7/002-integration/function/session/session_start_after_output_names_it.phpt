--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
session_start after output names where the body began
--FILE--
<?php
$sessDir = rtrim(sys_get_temp_dir(), "/") . "/phlwhere_" . getmypid();
@mkdir($sessDir);
session_save_path($sessDir);
$sessOutLog = [];
set_error_handler(function ($n, $s) use (&$sessOutLog) { $sessOutLog[] = "E$n: $s"; return true; });
echo "OUT\n";
var_dump(session_start());
var_dump(ini_set('session.name', 'z'));
restore_error_handler();
foreach ($sessOutLog as $sessOutLine) { echo str_replace(__FILE__, 'FILE', $sessOutLine), "\n"; }
?>
--EXPECT--
OUT
bool(false)
bool(false)
E2: session_start(): Session cannot be started after headers have already been sent (sent from FILE on line 7)
E2: ini_set(): Session ini settings cannot be changed after headers have already been sent (sent from FILE on line 7)
--CLEAN--
<?php
$sessDir = rtrim(sys_get_temp_dir(), "/") . "/phlwhere_" . getmypid();
foreach (glob($sessDir . "/sess_*") as $f) { @unlink($f); }
@rmdir($sessDir);
