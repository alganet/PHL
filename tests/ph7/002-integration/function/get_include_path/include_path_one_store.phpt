--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: include_path is ONE store — ini_set, set_include_path and the walk agree
--INI--
include_path=.
--FILE--
<?php
/* php keeps include_path in a single INI slot, so every way of reading it and
 * every way of writing it names the same value — and the include walk moves
 * with them. Pinned to `.` by --INI-- so the distro default cannot show. */
$ip_dir = sys_get_temp_dir() . '/phl_ip_' . getmypid();
@mkdir($ip_dir);
file_put_contents($ip_dir . '/phl_ip_target.php', "<?php return 'FOUND';\n");

printf("boot          get=%s ini=%s cfg=%s\n",
    var_export(get_include_path(), true),
    var_export(ini_get('include_path'), true),
    var_export(get_cfg_var('include_path'), true));

/* A write through ini_set() is what a bootstrap file does, and it has to move
 * the walk: this include finds nothing until the directive really changes. */
var_dump(@include 'phl_ip_target.php');
$ip_old = ini_set('include_path', $ip_dir);
printf("after ini_set old=%s get=%s ini=%s\n",
    var_export($ip_old, true),
    var_export(get_include_path() === $ip_dir, true),
    var_export(ini_get('include_path') === $ip_dir, true));
var_dump(include 'phl_ip_target.php');

/* And the other direction: set_include_path() has to be visible to ini_get(). */
$ip_old = set_include_path('.');
printf("after set     old=%s get=%s ini=%s\n",
    var_export($ip_old === $ip_dir, true),
    var_export(get_include_path(), true),
    var_export(ini_get('include_path'), true));

/* php registers the directive with OnUpdateStringUnempty: the empty string is
 * refused by BOTH writers and the value in force does not move. */
var_dump(set_include_path(''), ini_set('include_path', ''), get_include_path());

/* The string is handed back VERBATIM — a trailing slash and an empty segment
 * both survive, because each segment is what the walk prefixes to the name. */
$ip_raw = '/a/' . PATH_SEPARATOR . PATH_SEPARATOR . ' /b';
set_include_path($ip_raw);
var_dump(get_include_path() === $ip_raw,
    ini_get('include_path') === $ip_raw,
    ini_get_all(null, false)['include_path'] === $ip_raw);

/* ini_restore() puts back the value the child was started with. */
ini_restore('include_path');
var_dump(get_include_path(), ini_get('include_path'));

@unlink($ip_dir . '/phl_ip_target.php');
@rmdir($ip_dir);
?>
--EXPECT--
boot          get='.' ini='.' cfg='.'
bool(false)
after ini_set old='.' get=true ini=true
string(5) "FOUND"
after set     old=true get='.' ini='.'
bool(false)
bool(false)
string(1) "."
bool(true)
bool(true)
bool(true)
string(1) "."
string(1) "."
--CLEAN--
<?php
unset($ip_dir, $ip_old, $ip_raw);
