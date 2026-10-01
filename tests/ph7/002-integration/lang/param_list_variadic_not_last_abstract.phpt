--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter after a variadic one in a bodiless interface method is a compile fatal
--FILE--
<?php
interface I { function m(&...$a, ...$b); }
?>
--EXPECTF--
%s Fatal error:  Only the last parameter can be variadic in %s
--CLEAN--
<?php
