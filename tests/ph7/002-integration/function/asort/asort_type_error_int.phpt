--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with int argument should throw TypeError
--FILE--
<?php
$i = 42;
asort($i);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: asort(): Argument #1 ($array) must be of type array, int given in %s
--CLEAN--
<?php
unset($i);
