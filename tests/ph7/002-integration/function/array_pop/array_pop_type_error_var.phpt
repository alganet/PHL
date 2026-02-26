--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a non-array variable should raise a TypeError
--FILE--
<?php
$a = 'not an array';
array_pop($a);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_pop(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php
unset($a);
