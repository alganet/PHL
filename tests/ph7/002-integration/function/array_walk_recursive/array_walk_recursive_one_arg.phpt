--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with one argument throws ArgumentCountError
--FILE--
<?php
$a = array(1, 2);
array_walk_recursive($a);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_walk_recursive() expects at least 2 arguments, 1 given in %s
--CLEAN--
<?php
unset($a);
