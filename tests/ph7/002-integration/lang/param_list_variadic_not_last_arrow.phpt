--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter after a variadic one in an arrow function is a compile fatal
--FILE--
<?php
$f = fn(int ...$a, $b = 1) => 1;
?>
--EXPECTF--
%s Fatal error:  Only the last parameter can be variadic in %s
--CLEAN--
<?php
