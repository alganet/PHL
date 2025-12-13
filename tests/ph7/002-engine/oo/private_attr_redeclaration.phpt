--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Redeclaring private attribute in subclass emits a warning
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class A { private $x = 1; }
class B extends A { private $x = 2; }
?>
--EXPECTF--
%s 3 Warning: Private attribute 'A::x' redeclared inside child class 'B'
