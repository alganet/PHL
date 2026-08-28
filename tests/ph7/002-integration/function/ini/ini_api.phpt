--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: INI API — ini_get/ini_set/ini_restore/ini_get_all/get_cfg_var
--FILE--
<?php
$log = [];
// session ini mutations must precede any output (php headers rule)
$log[] = var_export(ini_get('session.name'), true);
$log[] = var_export(ini_set('session.name', 'X2'), true);
$log[] = var_export(ini_get('session.name'), true);
$log[] = var_export(session_name(), true);
$log[] = var_export(ini_restore('session.name'), true);
$log[] = var_export(ini_get('session.name'), true);
$log[] = var_export(ini_set('session.name', 'Y3'), true);
$log[] = var_export(get_cfg_var('session.name'), true);
$all = ini_get_all('session');
$log[] = var_export($all['session.name'], true);
// unknown keys
$log[] = var_export(ini_get('nonexistent.key'), true);
$log[] = var_export(ini_set('nonexistent.key', 'v'), true);
$log[] = var_export(get_cfg_var('nonexistent.key'), true);
// constants
$log[] = INI_USER . INI_PERDIR . INI_SYSTEM . INI_ALL;
// non-session round-trip that avoids install-dependent defaults
ini_set('memory_limit', '96M');
$log[] = var_export(ini_set('memory_limit', '64M'), true);
$log[] = var_export(ini_get('memory_limit'), true);
$log[] = var_export(ini_get('date.timezone'), true);
echo implode("\n", $log), "\n";
?>
--EXPECT--
'PHPSESSID'
'PHPSESSID'
'X2'
'X2'
NULL
'PHPSESSID'
'PHPSESSID'
'PHPSESSID'
array (
  'global_value' => 'PHPSESSID',
  'local_value' => 'Y3',
  'access' => 7,
)
false
false
false
1247
'96M'
'64M'
'UTC'
--CLEAN--
<?php
unset($log, $all);
