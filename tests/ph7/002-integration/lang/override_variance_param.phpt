--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override that narrows a parameter type (contravariance violation) is rejected
--DESCRIPTION--
Ran PHL-only while the refusal named the two methods and not their SIGNATURES.
php renders both declarations (F36); it runs on either engine now.
--FILE--
<?php
class OvpAnimal {}
class OvpDog extends OvpAnimal {}
class OvpBase { public function h(OvpAnimal $a): void {} }
class OvpC extends OvpBase { public function h(OvpDog $a): void {} }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvpC::h(OvpDog $a): void must be compatible with OvpBase::h(OvpAnimal $a): void%A
--CLEAN--
<?php
