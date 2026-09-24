--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a `const NAME = <expr>;` initializer is evaluated ONCE: the constant hands out the same value for ever, side effects included
--FILE--
<?php
// A `const` statement compiles its initializer to a bytecode program, and the
// engine used to RUN it on every read. For anything with an IDENTITY or a SIDE
// EFFECT that is a wrong answer rather than a slow one: the constant handed back
// a different object each time.
class CieCounter
{
    public static int $made = 0;
    public function __construct() { self::$made++; }
}
const CIE_OBJ = new CieCounter();

// Same object every time — the property php's own identity comparison rests on.
var_dump(CIE_OBJ === CIE_OBJ);
$cie_a = CIE_OBJ;
$cie_b = CIE_OBJ;
var_dump($cie_a === $cie_b);
var_dump(CIE_OBJ instanceof CieCounter, CIE_OBJ instanceof CieCounter);
// ...and the constructor ran ONCE, however many reads there were.
var_dump(CieCounter::$made);

// Every other reader of the same constant agrees, in either ORDER: the reader
// that runs first is the one that evaluates, and the rest get what it produced.
var_dump(constant('CIE_OBJ') === CIE_OBJ);
$cie_c = get_defined_constants()['CIE_OBJ'];
var_dump($cie_c === CIE_OBJ);
var_dump(CieCounter::$made);
class CieFirst
{
    public static int $made = 0;
    public function __construct() { self::$made++; }
}
const CIE_DUMPED_FIRST = new CieFirst();
$cie_d = get_defined_constants()['CIE_DUMPED_FIRST'];
var_dump($cie_d === CIE_DUMPED_FIRST, constant('CIE_DUMPED_FIRST') === $cie_d, CieFirst::$made);


// Scalar and array initializers keep answering what they always did.
const CIE_SUM = 1 + 2;
const CIE_STR = 'a' . 'b';
const CIE_ARR = [1, 2, ['k' => 3]];
var_dump(CIE_SUM, CIE_SUM + 1, CIE_STR, CIE_ARR[2]['k'], CIE_ARR === CIE_ARR);

// An expression over other constants is still an expression.
class CieHolder { const K = 3; }
const CIE_CALC = CieHolder::K * 2;
var_dump(CIE_CALC, CIE_CALC);

// define() was already value-backed and stays so.
define('CIE_DEFINED', new CieCounter());
var_dump(CIE_DEFINED === CIE_DEFINED);
var_dump(CieCounter::$made);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
int(1)
bool(true)
bool(true)
int(1)
bool(true)
bool(true)
int(1)
int(3)
int(4)
string(2) "ab"
int(3)
bool(true)
int(6)
int(6)
bool(true)
int(2)
--CLEAN--
<?php
unset($cie_a, $cie_b, $cie_c);
