--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors=stderr moves the DISPLAY copy to stderr and drops its blank line
--DESCRIPTION--
display_errors names a destination, not a truth value. With "stderr" the display
copy leaves program output entirely and goes to the error stream, and it loses
the leading blank line the stdout form opens with -- php writes this form with
fprintf() rather than through the output layer. log_errors is off here, so the
one line on stderr is the display copy: no `PHP ` prefix and ONE space after the
colon, where the log copy has two.
--INI--
display_errors=stderr
log_errors=0
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
Warning: Undefined array key "k" in %s on line %d
