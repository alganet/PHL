--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
true/false in unions and ?true resolve by value, not as a class (PHP 8.2)
--FILE--
<?php
function ulpTI(true|int $x) { return $x; }
function ulpQT(?true $x) { return $x; }
echo ulpTI(true) === true ? "ti_true_ok\n" : "ti_true_fail\n";
echo ulpTI(5) === 5 ? "ti_int_ok\n" : "ti_int_fail\n";
echo ulpQT(null) === null ? "qt_null_ok\n" : "qt_null_fail\n";
echo ulpQT(true) === true ? "qt_true_ok\n" : "qt_true_fail\n";
?>
--EXPECT--
ti_true_ok
ti_int_ok
qt_null_ok
qt_true_ok
--CLEAN--
<?php
