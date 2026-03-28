--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Private attribute redeclared in subclass
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class A {
    private $attr = 1;
}
class B extends A {
    private $attr = 2;
}
?>
--EXPECTF--
%s Warning:  Private attribute 'A::attr' redeclared inside child class 'B' %s
--CLEAN--
<?php

