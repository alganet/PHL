--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both gates on: display copy to stdout (leading blank line) AND log copy to stderr
--DESCRIPTION--
With display_errors=On and log_errors=On php emits both copies of a warning: the
display copy (`\nWarning: ... on line N`, one space, prefixed by a blank line) on
STDOUT, and the log copy (`PHP Warning:  ... on line N`, two spaces) on STDERR.
--INI--
display_errors=1
log_errors=1
--FILE--
<?php
echo "OUT\n";
$a = [];
$x = $a["k"];
echo "DONE\n";
?>
--EXPECTF--
OUT

Warning: Undefined array key "k" in %s on line %d
DONE
--EXPECT_STDERR--
PHP Warning:  Undefined array key "k" in %s on line %d
