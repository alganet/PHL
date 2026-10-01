--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The stderr display copy is outside the output layer: ob_start() never sees it
--DESCRIPTION--
Sent to stdout the display copy is program output and an output buffer captures
it. Sent to stderr it is not written through the output layer at all, so the
buffer closes empty and the diagnostic has already left on the error stream.
--INI--
display_errors=stderr
log_errors=0
--FILE--
<?php
ob_start();
$a = [];
$x = $a["k"];
var_dump(ob_get_clean());
?>
--EXPECT--
string(0) ""
--EXPECT_STDERR--
Warning: Undefined array key "k" in %s on line %d
