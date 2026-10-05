--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering an interface its parent implements is checked
--DESCRIPTION--
The abstract parent takes the interface without declaring the property, so
the property its subclass declares is the first to answer it.
--FILE--
<?php
interface PimTpI { public int $p { get; } }
abstract class PimTpA implements PimTpI { }
class PimTpC extends PimTpA { public ?int $p = null; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimTpC::$p must be subtype of int (as in class PimTpI) %s
