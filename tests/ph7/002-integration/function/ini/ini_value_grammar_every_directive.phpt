--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini runs its value grammar over every directive, not only error_reporting
--INI--
user_agent=E_ALL & ~E_DEPRECATED
from=E_NOTICE E_WARNING
default_mimetype=NoSuchConstantHere
arg_separator.input="E_ALL"
unserialize_callback_func=M_PI
arg_separator.output=On
--FILE--
<?php
// The bitwise expression is evaluated and its DECIMAL TEXT is what gets stored.
var_dump(ini_get('user_agent'));
// No operator: the pieces concatenate, each looked up as a constant first.
var_dump(ini_get('from'));
// An undefined name stands as its own text, not as the zero it reads as.
var_dump(ini_get('default_mimetype'));
// A quoted run is its literal bytes and never a constant lookup.
var_dump(ini_get('arg_separator.input'));
// php.ini is read before an extension registers a constant, so ext/standard's
// are names here even though the same text through parse_ini_file() is a value.
var_dump(ini_get('unserialize_callback_func'));
// A boolean word is a whole-value shape wherever the directive is a string one.
var_dump(ini_get('arg_separator.output'));
?>
--EXPECT--
string(5) "22527"
string(3) "8 2"
string(18) "NoSuchConstantHere"
string(5) "E_ALL"
string(4) "M_PI"
string(1) "1"
