--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class that leaves a method abstract gets php's shorter wording
--FILE--
<?php
/* php words this one differently for an anonymous class: there is no declaration
 * to put the word `abstract` on, so it says only what has to be implemented. */
abstract class AnonAbsBase { abstract public function z(); abstract public function y(); }
$x = new class extends AnonAbsBase {};
?>
--EXPECTF--
%s Fatal error:  Class AnonAbsBase@anonymous must implement 2 abstract methods (AnonAbsBase::z, AnonAbsBase::y) %s
--CLEAN--
<?php
