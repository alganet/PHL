--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An abstract method may not contain a body
--DESCRIPTION--
php words this as the declaration's problem rather than a missing token; an
interface method that grows one gets the same sentence with php's other noun.
--FILE--
<?php
abstract class AmbC { abstract function f() {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Abstract function AmbC::f() cannot contain body %s
--CLEAN--
<?php
