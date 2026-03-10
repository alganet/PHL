--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with non-array first argument throws TypeError
--FILE--
<?php
$x = 'not_array';
array_walk_recursive($x, function($v, $k) {});
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_walk_recursive(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php
unset($x);
