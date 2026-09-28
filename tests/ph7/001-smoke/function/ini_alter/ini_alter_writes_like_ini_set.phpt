--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_alter writes a directive and answers the old value
--FILE--
<?php
$iniAlterOld = ini_alter('precision', '12');
var_dump($iniAlterOld, ini_get('precision'));
var_dump(ini_alter('precision', $iniAlterOld), ini_get('precision'));
var_dump(ini_alter('no_such_directive', '1'));
try { ini_alter('precision'); } catch (Throwable $iniAlterErr) { echo get_class($iniAlterErr), ": ", $iniAlterErr->getMessage(), "\n"; }
?>
--EXPECT--
string(2) "14"
string(2) "12"
string(2) "12"
string(2) "14"
bool(false)
ArgumentCountError: ini_alter() expects exactly 2 arguments, 1 given
--CLEAN--
<?php
unset($iniAlterOld, $iniAlterErr);
