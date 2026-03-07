--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() with non-array argument throws TypeError
--FILE--
<?php
$x = 'hello';
array_unshift($x, 'a');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_unshift(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php
unset($x);
