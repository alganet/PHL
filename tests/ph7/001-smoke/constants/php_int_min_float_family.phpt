--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP_INT_MIN (7.0) and the PHP_FLOAT_* family (7.2) constant values
--FILE--
<?php
echo var_export(PHP_INT_MIN, true), "\n";
echo var_export(PHP_INT_MIN === -PHP_INT_MAX - 1, true), "\n";
echo var_export(PHP_FLOAT_EPSILON, true), "\n";
echo var_export(PHP_FLOAT_MAX, true), "\n";
echo var_export(PHP_FLOAT_MIN, true), "\n";
echo var_export(PHP_FLOAT_DIG, true), "\n";
echo var_export(1.0 + PHP_FLOAT_EPSILON !== 1.0, true), "\n";
?>
--EXPECT--
-9223372036854775807-1
true
2.220446049250313E-16
1.7976931348623157E+308
2.2250738585072014E-308
15
true
--CLEAN--
<?php
