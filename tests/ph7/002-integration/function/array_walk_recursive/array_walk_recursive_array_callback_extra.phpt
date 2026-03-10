--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk_recursive with array callback having more than two members throws TypeError
--FILE--
<?php
class Foo { function bar($v, $k) {} }
$a = array(1, 2);
$obj = new Foo();
array_walk_recursive($a, array($obj, 'bar', 'extra'));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_walk_recursive(): Argument #2 ($callback) must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php
unset($a, $obj);
