--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property composed from a trait answering an interface is checked
--DESCRIPTION--
The property the class takes from a trait is the class's own by the time the
interface is implemented, and php names the class.
--FILE--
<?php
interface PimTrI { public int $p { get; } }
trait PimTrT { public ?int $p = null; }
class PimTrC implements PimTrI { use PimTrT; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimTrC::$p must be subtype of int (as in class PimTrI) %s
