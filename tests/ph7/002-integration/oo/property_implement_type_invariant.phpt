--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering a get-and-set interface property keeps its type exactly
--DESCRIPTION--
With both hooks the interface property is read and written, so the type is
invariant: `?int` under `int` is refused with no "subtype of".
--FILE--
<?php
interface PimInI { public int $p { get; set; } }
class PimInC implements PimInI { public ?int $p = null; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimInC::$p must be int (as in class PimInI) %s
