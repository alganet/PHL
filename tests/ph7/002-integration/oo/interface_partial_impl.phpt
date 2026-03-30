--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Partial interface implementation with correct origin for missing method
--FILE--
<?php
interface I { public function a(); public function b(); }
abstract class Base implements I { public function a() { return 1; } }
class Child extends Base { }
?>
--EXPECTF--
%s %s %s  Class Child contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (I::b) %s
--CLEAN--
<?php
