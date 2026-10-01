--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error_reporting ini value is an expression over the bitwise operators
--DESCRIPTION--
A php.ini value is read by an expression grammar: `|`, `&`, `^`, unary `~` and
`!` over operands in which a bare identifier stands for the constant of that
name. The directive below is therefore E_ALL with two bits cleared, and what
gets STORED is the number the parser made of it rather than the letters that
were typed. The warning is in the mask and prints; the two user-level
diagnostics were cleared by the `~` and stay quiet.
--INI--
error_reporting=E_ALL & ~(E_USER_DEPRECATED|E_USER_NOTICE)
display_errors=1
log_errors=0
--FILE--
<?php
var_dump(error_reporting());
var_dump(ini_get('error_reporting'));
var_dump(error_reporting() === (E_ALL & ~(E_USER_DEPRECATED | E_USER_NOTICE)));
$a = [];
$x = $a['gone'];                                  // E_WARNING: in the mask
trigger_error('hushed', E_USER_NOTICE);           // cleared by the ~
trigger_error('also hushed', E_USER_DEPRECATED);  // cleared by the ~
echo "END\n";
?>
--EXPECTF--
int(13311)
string(5) "13311"
bool(true)

Warning: Undefined array key "gone" in %s on line %d
END
