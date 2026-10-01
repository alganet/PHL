--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error_reporting ini value names a constant, and the parser stores its number
--DESCRIPTION--
A php.ini value is read by an expression grammar, not taken as a literal: a bare
identifier stands for the constant of that name. So the directive below is 2 --
the warning prints and the user notice beside it does not -- and ini_get() shows
what the parser MADE of the value rather than the letters that were typed, which
is also what ini_restore() has to come back to.
--INI--
error_reporting=E_WARNING
display_errors=1
log_errors=0
--FILE--
<?php
var_dump(error_reporting());
var_dump(ini_get('error_reporting'));
$a = [];
$x = $a['gone'];                                  // E_WARNING: in the mask
trigger_error('quiet', E_USER_NOTICE);            // E_USER_NOTICE: masked out
error_reporting(E_ALL);
var_dump(error_reporting(), ini_get('error_reporting'));
ini_restore('error_reporting');
var_dump(error_reporting(), ini_get('error_reporting'));
echo "END\n";
?>
--EXPECTF--
int(2)
string(1) "2"

Warning: Undefined array key "gone" in %s on line %d
int(30719)
string(5) "30719"
int(2)
string(1) "2"
END
