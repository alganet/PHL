--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with array callback throws TypeError
--FILE--
<?php
$a = array(1, 2);
array_walk_recursive($a, array(1));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_walk_recursive(): Argument #2 ($callback) must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php
unset($a);
