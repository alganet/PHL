--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An inherited property answering an interface is checked under its own class
--DESCRIPTION--
The class implements the interface but declares nothing; the property it
inherits from its parent answers the interface, and php names the PARENT as
the class whose property has the wrong type, on the implementing class's line.
--FILE--
<?php
interface PimHeI { public int $p { get; } }
class PimHeP { public ?int $p = null; }
class PimHeC extends PimHeP implements PimHeI { }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimHeP::$p must be subtype of int (as in class PimHeI) %s
