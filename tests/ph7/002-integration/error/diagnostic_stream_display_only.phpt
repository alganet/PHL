--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors on + log_errors off writes only the stdout display copy
--DESCRIPTION--
The mirror of the stock gates: the display copy goes to STDOUT and STDERR stays
clean.
--INI--
display_errors=1
log_errors=0
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
