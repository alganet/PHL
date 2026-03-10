--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk with undefined function name throws TypeError
--FILE--
<?php
$a = array(1, 2);
array_walk($a, 'nonexistent_func');
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_walk(): Argument #2 ($callback) must be a valid callback, function "nonexistent_func" not found or invalid function name in %s
--CLEAN--
<?php
unset($a);
