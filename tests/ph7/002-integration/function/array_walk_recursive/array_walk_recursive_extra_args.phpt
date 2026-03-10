--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with too many arguments throws ArgumentCountError
--FILE--
<?php
$a = array(1, 2);
array_walk_recursive($a, function($v, $k) {}, 'extra1', 'extra2');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_walk_recursive() expects at most 3 arguments, 4 given in %s
--CLEAN--
<?php
unset($a);
