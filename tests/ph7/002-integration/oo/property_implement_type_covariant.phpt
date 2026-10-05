--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property answering a get-only interface property may only narrow its type
--DESCRIPTION--
An interface property with only a get hook is read and never written, so the
implementing class may narrow its type but not widen it: `?int` under `int` is
refused, and php says "subtype of".
--FILE--
<?php
interface PimCoI { public int $p { get; } }
class PimCoC implements PimCoI { public ?int $p = null; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PimCoC::$p must be subtype of int (as in class PimCoI) %s
