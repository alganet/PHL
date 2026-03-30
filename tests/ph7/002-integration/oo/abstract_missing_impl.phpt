--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Abstract method missing implementation produces fatal error
--FILE--
<?php
abstract class Base { abstract public function doSomething(); }
class Child extends Base { }
?>
--EXPECTF--
%s Fatal error:  Class Child contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (Base::doSomething) %s
--CLEAN--
<?php

