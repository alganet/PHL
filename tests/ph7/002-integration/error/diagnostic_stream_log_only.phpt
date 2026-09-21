--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Stock CLI streams: display_errors off + log_errors on writes only the stderr log copy
--DESCRIPTION--
A runtime warning under the stock CLI gates (display_errors=Off, log_errors=On)
leaves program STDOUT clean and writes the `PHP Warning:  ...` log copy to
STDERR — so a consumer parsing stdout is not polluted and 2>/dev/null cannot be
relied on to carry it. --EXPECT_STDERR-- captures the two streams separately.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
echo "OUT\n";
$a = [];
$x = $a["k"];
echo "DONE\n";
?>
--EXPECT--
OUT
DONE
--EXPECT_STDERR--
PHP Warning:  Undefined array key "k" in %s on line %d
