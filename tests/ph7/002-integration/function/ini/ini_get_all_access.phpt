--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_get_all()['access'] is php's own modifiable mask, per directive
--FILE--
<?php
// The mask says WHERE a directive may be set -- 1 USER, 2 PERDIR, 4 SYSTEM, 7
// ALL -- and a program reads it to decide whether ini_set() can work at all.
// Three of this engine's rows disagreed with php: allow_url_fopen was
// PERDIR|SYSTEM where php has SYSTEM alone, arg_separator.input was ALL where
// php has PERDIR|SYSTEM (so a script may not set it), and session.auto_start
// was PERDIR|SYSTEM where php has PERDIR.
$igaa = ini_get_all();
foreach (['allow_url_fopen', 'allow_url_include', 'arg_separator.input',
          'arg_separator.output', 'session.auto_start', 'session.name',
          'precision', 'memory_limit', 'include_path'] as $igaaName) {
    printf("%-22s %d\n", $igaaName, $igaa[$igaaName]['access']);
}
// A directive a script may not touch refuses the write and says so.
$igaaWarn = [];
set_error_handler(function ($n, $s) use (&$igaaWarn) { $igaaWarn[] = $s; return true; });
var_dump(ini_set('arg_separator.input', ';'));
restore_error_handler();
var_dump(ini_get('arg_separator.input'));
?>
--EXPECT--
allow_url_fopen        4
allow_url_include      4
arg_separator.input    6
arg_separator.output   7
session.auto_start     2
session.name           7
precision              7
memory_limit           7
include_path           7
bool(false)
string(1) "&"
--CLEAN--
<?php
