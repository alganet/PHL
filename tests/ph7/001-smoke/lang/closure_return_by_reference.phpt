--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure returns by reference, and the slot it names outlives the call
--DESCRIPTION--
`function &(…) {…}` puts the `&` exactly where the named form does, and nothing consumed it,
so every by-ref closure was `syntax error, unexpected token "&", expecting "("`. Underneath,
the caller now binds to the very slot the `return` named -- a local, a parameter or an
element of a local array -- which php keeps alive by refcount and this engine pins. It used
to DROP the reference for a local, behind two notices php does not have, and leave the caller
aimed at an element the frame had already released: `&f($x){ $a = [$x]; return $a[0]; }`
handed back NULL.
--FILE--
<?php
$crbrH = function &(array &$a) { return $a['k']; };
$crbrArr = ['k' => 1];
$crbrR = &$crbrH($crbrArr);
$crbrR = 9;
var_dump($crbrArr['k']);
$crbrS = static function &(array &$a) { return $a['k']; };
$crbrArr2 = ['k' => 2];
$crbrR2 = &$crbrS($crbrArr2);
$crbrR2 = 8;
var_dump($crbrArr2['k']);
$crbrC = function &() { static $v = 1; return $v; };
$crbrR3 = &$crbrC();
$crbrR3 = 5;
var_dump($crbrC());
var_dump((new ReflectionFunction($crbrC))->returnsReference());
function crbrElem($x) { $a = [$x]; return $a[0]; }
function &crbrElemRef($x) { $a = [$x]; return $a[0]; }
$crbrO = new stdClass;
$crbrR4 = &crbrElemRef($crbrO);
var_dump($crbrR4 instanceof stdClass);
function &crbrParam($x) { return $x; }
$crbrR5 = &crbrParam($crbrO);
var_dump($crbrR5 instanceof stdClass);
function &crbrLocal() { $v = 5; return $v; }
$crbrR6 = &crbrLocal();
var_dump($crbrR6);
$crbrOuter = new stdClass;
$crbrOuter->p = 1;
$crbrU = function &() use ($crbrOuter) { return $crbrOuter->p; };
$crbrR7 = &$crbrU();
$crbrR7 = 4;
var_dump($crbrOuter->p);
?>
--EXPECT--
int(9)
int(8)
int(5)
bool(true)
bool(true)
bool(true)
int(5)
int(4)
--CLEAN--
<?php
