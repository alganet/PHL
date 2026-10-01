--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
display_errors=stderr with log_errors on puts BOTH copies on stderr, log first
--DESCRIPTION--
The two copies are still two, and still differ: the log copy carries the `PHP `
prefix and two spaces after the colon, the display copy neither. They share the
stream here, and the log copy is written first.
--INI--
display_errors=stderr
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
Warning: Undefined array key "k" in %s on line %d
