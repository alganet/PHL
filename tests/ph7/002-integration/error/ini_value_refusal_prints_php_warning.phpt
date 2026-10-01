--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A -d value the grammar only partly takes now prints php's own syntax-error warning too
--DESCRIPTION--
php's yacc grammar reduces `error_reporting=E_ALL)` to a complete value (30719)
before it ever sees the ')' -- the assignment commits, and only then does the
parser discover ')' cannot start anything else, so it separately warns straight
to stderr with none of error_reporting/display_errors/log_errors gating it (any
of the three could be the directive being refused). One --INI-- line here is
exactly one `-d`, landing on php's own virtual line 6: its CLI SAPI joins every
-d into one buffer behind five lines of its own hardcoded startup ini before the
first one (php_cli.c's HARDCODED_INI). PHL now matches both the stored value and
the warning.
--INI--
error_reporting=E_ALL)
--FILE--
<?php
var_dump(ini_get('error_reporting'));
?>
--EXPECTF--
PHP:  syntax error, unexpected ')' in Unknown on line 6
string(5) "30719"
