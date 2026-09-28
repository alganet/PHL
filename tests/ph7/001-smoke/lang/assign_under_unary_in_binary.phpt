--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An assignment under a prefix unary binds inside a binary operand
--DESCRIPTION--
php reads `c && !$d = f()` as `c && !($d = f())`, exactly as it reads the unparenthesised
`!$d = f()` -- every prefix unary covers the whole assignment rather than just its target.
The assignment pass walked the left operand's right spine looking for an lvalue and stopped
at the unary (a unary node has no right child), so the whole shape was a syntax error.
--FILE--
<?php
function aubiF($v = 1) { return $v; }
if (true && !$aubiA = aubiF(2)) { }
var_dump($aubiA);
if (false || !$aubiB = aubiF(3)) { }
var_dump($aubiB);
if (true and !$aubiC = aubiF(4)) { }
var_dump($aubiC);
if (true && !!$aubiD = aubiF(5)) { }
var_dump($aubiD);
if (true && true && !$aubiE = aubiF(6)) { }
var_dump($aubiE);
var_dump(true && -$aubiG = aubiF(7));
var_dump($aubiG);
$aubiR = true && !$aubiH = aubiF(8);
var_dump($aubiH);
while (false !== $aubiI = aubiF(9)) { break; }
var_dump($aubiI);
if (!$aubiJ = aubiF(10)) { }
var_dump($aubiJ);
?>
--EXPECT--
int(2)
int(3)
int(4)
int(5)
int(6)
bool(true)
int(7)
int(8)
int(9)
int(10)
--CLEAN--
<?php
