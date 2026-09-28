--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A declaration inside a block binds when execution REACHES it, as php's does
--FILE--
<?php
/* php binds a function or class written at a unit's TOP LEVEL when the unit
 * compiles, and one written anywhere else -- inside an `if`, a loop, a `try`, a
 * `switch`, another function's body -- when execution REACHES it. Binding the
 * second kind early is what made `if (!function_exists('x')) { function x(){} }`
 * REPLACE a builtin: that is how every symfony/polyfill-* package ships, so a
 * tree with one silently lost the engine's own mbstring, ctype and str_* to
 * userland back-ports. */
echo "-- a guard that is already satisfied declares nothing\n";
if (!function_exists('strlen')) { function strlen($s) { return 'POLYFILL'; } }
var_dump(strlen('abc'));
if (!class_exists('ArrayObject')) { class ArrayObject { public $poly = 1; } }
var_dump((new ArrayObject([1,2]))->count());

echo "-- a branch that never runs declares nothing\n";
if (false) { function cdbNever() { return 1; } }
if (false) { class CdbNeverC {} }
if (false) { interface CdbNeverI {} }
if (false) { trait CdbNeverT {} }
var_dump(function_exists('cdbNever'), class_exists('CdbNeverC', false),
         interface_exists('CdbNeverI', false), trait_exists('CdbNeverT', false));

echo "-- a branch that DOES run declares it, once\n";
if (true) { function cdbCond() { return 'cond'; } }
foreach ([1] as $i) { function cdbLoop() { return 'loop'; } }
try { function cdbTry() { return 'try'; } } catch (Throwable $e) {}
switch (1) { case 1: function cdbSwitch() { return 'switch'; } }
while (!function_exists('cdbWhile')) { function cdbWhile() { return 'while'; } }
var_dump(cdbCond(), cdbLoop(), cdbTry(), cdbSwitch(), cdbWhile());

echo "-- a nested one waits for its enclosing call\n";
function cdbOuter() { function cdbInner() { return 'inner'; } return 'outer'; }
var_dump(function_exists('cdbInner'));
var_dump(cdbOuter(), function_exists('cdbInner'), cdbInner());

echo "-- ...and so does a class\n";
function cdbMake() { if (!class_exists('CdbLazy')) { class CdbLazy { public $v = 9; } } return (new CdbLazy)->v; }
var_dump(class_exists('CdbLazy', false));
var_dump(cdbMake(), cdbMake(), class_exists('CdbLazy', false));

echo "-- a TOP-LEVEL one is still hoisted\n";
var_dump(cdbHoisted(), new CdbHoistedC instanceof CdbHoistedC);
function cdbHoisted() { return 'hoisted'; }
class CdbHoistedC {}

echo "-- the guard sees the engine's own names\n";
var_dump(function_exists('mb_strlen'), function_exists('ctype_digit'),
         class_exists('ArrayIterator'), class_exists('DateTime'));
--EXPECT--
-- a guard that is already satisfied declares nothing
int(3)
int(2)
-- a branch that never runs declares nothing
bool(false)
bool(false)
bool(false)
bool(false)
-- a branch that DOES run declares it, once
string(4) "cond"
string(4) "loop"
string(3) "try"
string(6) "switch"
string(5) "while"
-- a nested one waits for its enclosing call
bool(false)
string(5) "outer"
bool(true)
string(5) "inner"
-- ...and so does a class
bool(false)
int(9)
int(9)
bool(true)
-- a TOP-LEVEL one is still hoisted
string(7) "hoisted"
bool(true)
-- the guard sees the engine's own names
bool(true)
bool(true)
bool(true)
bool(true)
