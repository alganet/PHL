--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mixed property type accepts any value, including null (PHP 8.0)
--FILE--
<?php
class MixedPropHolder { public mixed $m = 0; }
$mp = new MixedPropHolder();
$mp->m = 5;    echo is_int($mp->m) ? "int_ok\n" : "int_fail\n";
$mp->m = "s";  echo is_string($mp->m) ? "str_ok\n" : "str_fail\n";
$mp->m = null; echo ($mp->m === null) ? "null_ok\n" : "null_fail\n";
$mp->m = [1];  echo is_array($mp->m) ? "arr_ok\n" : "arr_fail\n";
?>
--EXPECT--
int_ok
str_ok
null_ok
arr_ok
--CLEAN--
<?php
unset($mp);
