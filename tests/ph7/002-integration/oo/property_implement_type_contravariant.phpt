--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering a set-only interface property may only widen its type
--DESCRIPTION--
An interface property with only a set hook is written and never read, so the
implementing class may widen its type but not narrow it: `int` under `?int`
is refused, and php says "supertype of".
--FILE--
<?php
interface PimCtI { public ?int $p { set; } }
class PimCtC implements PimCtI { public int $p = 0; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimCtC::$p must be supertype of ?int (as in class PimCtI) %s
