--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors/log_errors/error_log default to the stock CLI values
--DESCRIPTION--
The stock CLI ini view: display_errors is Off (empty string), log_errors is On
("1"), error_log is unset (empty string) — so a program's diagnostics go to
stderr and its stdout stays clean by default.
--FILE--
<?php
var_dump(ini_get("display_errors"));
var_dump(ini_get("log_errors"));
var_dump(ini_get("error_log"));
?>
--EXPECT--
string(0) ""
string(1) "1"
string(0) ""
