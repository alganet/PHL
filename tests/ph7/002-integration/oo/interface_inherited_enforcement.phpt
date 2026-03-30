--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface method origin tracked through abstract parent classes
--FILE--
<?php
interface I { public function foo(); }
abstract class Base implements I { }
class Child extends Base { }
?>
--EXPECTF--
%s %s %s  Class Child contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (I::foo) %s
--CLEAN--
<?php
