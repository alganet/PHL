--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Uncaught report prints every frame of a nested call chain
--DESCRIPTION--
Regression: the uncaught renderer described only the innermost frame, so the
intermediate frames of a nested chain vanished from the report even though the
exception's own trace (getTraceAsString) had them all along.
--FILE--
<?php
function utfd2() { throw new Exception('deep'); }
function utfd1() { utfd2(); }
utfd1();
?>
--EXPECTF--
%APHP Fatal error:  Uncaught Exception: deep in %s:2
Stack trace:
#0 %s(3): utfd2()
#1 %s(4): utfd1()
#2 {main}
  thrown in %s on line 2%A
