--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface properties php accepts the implementation of
--DESCRIPTION--
Each shape is a type the interface's variance allows, a class or `self` type
resolved on both sides, a property the parent already answered, or two
interfaces whose types the one property satisfies at once.
--FILE--
<?php
interface PimOkI { public int $a { get; } }
class PimOkRo implements PimOkI { public readonly int $a; function __construct() { $this->a = 1; } }
class PimOkHook implements PimOkI { public int $a { get => 2; } }
interface PimOkM { public mixed $m { get; set; } }
class PimOkMix implements PimOkM { public mixed $m = 3; }
interface PimOkS { public self $s { get; } }
class PimOkSelf implements PimOkS { public ?PimOkSelf $x = null; public PimOkSelf $s; }
interface PimOkN { public ?int $n { get; } }
interface PimOkN2 extends PimOkN { public int $n { get; } }
class PimOkNarrow implements PimOkN2 { public int $n = 4; }
abstract class PimOkAbs implements PimOkI { public int $a = 5; }
class PimOkKid extends PimOkAbs { public int $a = 6; }
class PimOkAgain extends PimOkAbs implements PimOkI { }
class PimOkPset implements PimOkI { public private(set) int $a = 7; }
interface PimOkW { public int|string $w { set; } }
class PimOkWide implements PimOkW { public int|string|float $w = 8.5; }
class PimOkBoth implements PimOkI, PimOkN { public int $a = 9; public int $n = 10; }
class PimOkGrand extends PimOkHook { }
var_dump((new PimOkRo)->a, (new PimOkHook)->a, (new PimOkMix)->m, (new PimOkNarrow)->n,
    (new PimOkKid)->a, (new PimOkAgain)->a, (new PimOkPset)->a, (new PimOkWide)->w,
    (new PimOkBoth)->n, (new PimOkGrand)->a);
?>
--EXPECT--
int(1)
int(2)
int(3)
int(4)
int(6)
int(5)
int(7)
float(8.5)
int(10)
int(2)
