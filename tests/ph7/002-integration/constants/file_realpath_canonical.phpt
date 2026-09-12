--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__FILE__ / ReflectionClass::getFileName() report the realpath'd absolute path (symlinks and '..' resolved), matching php
--FILE--
<?php
$cnDir = sys_get_temp_dir() . '/phl_canon_' . getmypid();
@mkdir($cnDir);
@mkdir($cnDir . '/sub');
file_put_contents($cnDir . '/inc.php', "<?php function cnf() { return __FILE__; } class CnClass {}\n");
// Include through a non-canonical path (…/sub/../inc.php)
require $cnDir . '/sub/../inc.php';
$cnDirect = realpath($cnDir . '/inc.php');
var_dump(cnf() === $cnDirect);
var_dump((new ReflectionClass('CnClass'))->getFileName() === $cnDirect);
var_dump(strpos(cnf(), '/../') === false);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
$cnDir = sys_get_temp_dir() . '/phl_canon_' . getmypid();
@unlink($cnDir . '/inc.php');
@rmdir($cnDir . '/sub');
@rmdir($cnDir);
