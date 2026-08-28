--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session lifecycle (band D) — start/write/read/regenerate/destroy
--FILE--
<?php
$log = [];
$dir = rtrim(sys_get_temp_dir(), "/") . "/phlsess_" . getmypid();
@mkdir($dir);
session_save_path($dir);
$log[] = session_status() . "|" . PHP_SESSION_NONE . PHP_SESSION_ACTIVE . PHP_SESSION_DISABLED;
$log[] = session_name() . "|" . session_id() . "|";
$log[] = var_export(session_start(), true);
$log[] = session_status();
$log[] = strlen(session_id()) . (preg_match("/^[0-9a-v]+$/", session_id()) ? "ok" : "?");
$_SESSION["user"] = "alice";
$_SESSION["n"] = 42;
$_SESSION["arr"] = [1, "b" => 2];
$id = session_id();
$log[] = var_export(session_write_close(), true);
$log[] = session_status();
$log[] = file_get_contents($dir . "/sess_" . $id);
$log[] = var_export($_SESSION, true);
$log[] = var_export(session_id($id) !== false, true);
$log[] = var_export(session_start(), true);
$log[] = var_export($_SESSION, true);
$log[] = var_export(session_regenerate_id(), true);
$log[] = var_export(session_id() !== $id, true);
$new = session_id();
$log[] = var_export(session_destroy(), true);
$log[] = session_status() . "|" . session_id() . "|";
$log[] = var_export(file_exists($dir . "/sess_" . $new), true);
$log[] = var_export(session_unset(), true);
$log[] = var_export(isset($_SESSION["user"]), true);
echo implode("\n", $log), "\n";
?>
--EXPECT--
1|120
PHPSESSID||
true
2
32ok
true
1
user|s:5:"alice";n|i:42;arr|a:2:{i:0;i:1;s:1:"b";i:2;}
array (
  'user' => 'alice',
  'n' => 42,
  'arr' => 
  array (
    0 => 1,
    'b' => 2,
  ),
)
true
true
array (
  'user' => 'alice',
  'n' => 42,
  'arr' => 
  array (
    0 => 1,
    'b' => 2,
  ),
)
true
true
true
1||
false
false
true
--CLEAN--
<?php
unset($log, $dir, $id, $new);
