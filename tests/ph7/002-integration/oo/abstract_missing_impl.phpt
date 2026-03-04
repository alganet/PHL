--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Abstract method missing implementation should warn and cause error on instantiation
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
abstract class Base { abstract public function doSomething(); }
class Child extends Base { }
new Child();
?>
--EXPECTF--
%s %d Warning:  Abstract method 'Base:doSomething' must be defined inside child class 'Child'
--CLEAN--
<?php

