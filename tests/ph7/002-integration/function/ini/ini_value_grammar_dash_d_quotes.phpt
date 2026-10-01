--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
-d quotes a value that does not open on an alphanumeric, so its text stands
--INI--
user_agent=(E_ALL) ^ E_NOTICE
from=~~2
default_mimetype=~E_NOTICE
arg_separator.input=_E_ALL
--FILE--
<?php
// -d takes its argument from argv and wraps it in quotes unless it opens on
// an alphanumeric or a quote, so a leading paren, a leading unary and a leading
// underscore all reach the grammar as a quoted run and store their own text.
var_dump(ini_get('user_agent'));
var_dump(ini_get('from'));
var_dump(ini_get('default_mimetype'));
var_dump(ini_get('arg_separator.input'));
?>
--EXPECT--
string(18) "(E_ALL) ^ E_NOTICE"
string(3) "~~2"
string(9) "~E_NOTICE"
string(6) "_E_ALL"
