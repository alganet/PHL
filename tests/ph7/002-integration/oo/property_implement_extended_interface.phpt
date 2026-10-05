--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering an interface's parent interface is checked
--DESCRIPTION--
The property is declared by the interface the implemented one extends.
--FILE--
<?php
interface PimExI { public int $p { get; } }
interface PimExJ extends PimExI { }
class PimExC implements PimExJ { public ?int $p = null; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimExC::$p must be subtype of int (as in class PimExI) %s
