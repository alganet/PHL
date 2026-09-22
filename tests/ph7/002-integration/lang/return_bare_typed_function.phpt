--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a bare return in a function that declares a return type is a COMPILE error
--FILE--
<?php
// php never runs this program: the check happens while the function is
// compiled, so the echo below is never reached (a runtime TypeError would
// have printed it first).
echo "unreachable\n";
function bad(): int { return; }
?>
--EXPECTF--
%s Fatal error:  A function with return type must return a value in %s
--CLEAN--
<?php
