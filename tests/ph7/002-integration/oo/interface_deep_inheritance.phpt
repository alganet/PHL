--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface origin tracked through deep abstract class chain
--FILE--
<?php
interface I { public function x(); }
abstract class A implements I { }
abstract class B extends A { }
class C extends B { }
?>
--EXPECTF--
%s %s %s  Class C contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (I::x) %s
--CLEAN--
<?php
