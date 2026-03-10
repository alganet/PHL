--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with non-array first argument throws TypeError
--FILE--
<?php
$x = 5;
array_splice($x, 0);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_splice(): Argument #1 ($array) must be of type array, int given %s
--CLEAN--
<?php
unset($x);
