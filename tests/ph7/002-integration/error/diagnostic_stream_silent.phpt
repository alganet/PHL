--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Both gates off: a runtime warning is emitted to neither stream
--DESCRIPTION--
display_errors=Off and log_errors=Off silences the built-in printer entirely
(the diagnostic still reaches a user error handler and error_get_last(); only the
default-processing copies are gated).
--INI--
display_errors=0
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
