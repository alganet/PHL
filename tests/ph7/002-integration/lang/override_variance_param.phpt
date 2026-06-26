--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override that narrows a parameter type (contravariance violation) is rejected
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-specific diagnostic wording'; ?>
--FILE--
<?php
class OvpAnimal {}
class OvpDog extends OvpAnimal {}
class OvpBase { public function h(OvpAnimal $a): void {} }
class OvpC extends OvpBase { public function h(OvpDog $a): void {} }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvpC::h() must be compatible with OvpBase::h()%A
--CLEAN--
<?php
