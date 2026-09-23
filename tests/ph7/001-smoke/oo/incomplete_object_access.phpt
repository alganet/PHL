--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Touching an incomplete object: warning to read, Error to modify or call
--DESCRIPTION--
php's __PHP_Incomplete_Class handlers split three ways, and PHL used to have
none of them (the carrier could not even exist). Every READ shape — a plain
read, isset()/empty(), `??`, a by-value argument, the magic name member itself —
is an E_WARNING whose body carries the docref qualifier of the RAISING function
(`incoRead(): The script tried to access a property on an incomplete object...`)
and answers NULL. Every WRITE shape — store, unset(), ++/compound, a reference
fetch or target, a subscript-write base, a by-ref argument — is the catchable
modify Error. A method call is the call Error wherever it is spelled: the direct
call, the first-class callable (resolved at CREATION, php's order),
method_exists() on the instance, and — php's own quirk — is_callable() in ARRAY
form; while is_callable($obj), the syntax_only form, call_user_func() and
Closure::fromCallable() ask differently and answer with the ordinary callback
taxonomy instead. The diagnostics name the ORIGINAL class from the payload, and
none of it disturbs the payload: serialize() still writes the original bytes.
--FILE--
<?php
set_error_handler(function ($n, $s) { echo '  [', $n, '] ', $s, "\n"; return true; });
function incoShow($label, $fn) {
    try { $out = $fn(); echo $label, ' => ', str_replace("\n", '', var_export($out, true)), "\n"; }
    catch (Throwable $e) { echo $label, ' !! ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
$incoObj = unserialize('O:7:"IncoFoo":2:{s:1:"x";i:5;s:4:"name";s:3:"abc";}');

/* Named wrappers: php's docref warning is prefixed with the RAISING function's
 * name, and a closure's name carries its file and line since 8.4 — a named
 * function keeps the expectation portable (and proves the prefix is right). */
function incoRead($o)      { return $o->x ?? 'was-null'; }
function incoIsset($o)     { return isset($o->x); }
function incoEmpty($o)     { return empty($o->x); }
function incoCoal($o)      { return $o->zz ?? 'dflt'; }
function incoNameM($o)     { return isset($o->__PHP_Incomplete_Class_Name); }
function incoByValTake($p) { return $p ?? 'was-null'; }
function incoByVal($o)     { return incoByValTake($o->x); }
function incoCoalAssign($o){ $o->zz ??= 1; }
class IncoMeth { public function go($o) { return $o->x ?? 'was-null'; } }

echo "-- every READ shape is the access warning answering NULL\n";
incoShow('read', fn() => incoRead($GLOBALS['incoObj']));
incoShow('isset', fn() => incoIsset($GLOBALS['incoObj']));
incoShow('empty', fn() => incoEmpty($GLOBALS['incoObj']));
incoShow('coalesce', fn() => incoCoal($GLOBALS['incoObj']));
incoShow('name member', fn() => incoNameM($GLOBALS['incoObj']));
incoShow('by-value arg', fn() => incoByVal($GLOBALS['incoObj']));
incoShow('inside a method', fn() => (new IncoMeth)->go($GLOBALS['incoObj']));

echo "-- every WRITE shape is the modify Error\n";
incoShow('store', function () use ($incoObj) { $incoObj->x = 9; });
incoShow('unset', function () use ($incoObj) { unset($incoObj->x); });
incoShow('increment', function () use ($incoObj) { $incoObj->x++; });
incoShow('compound', function () use ($incoObj) { $incoObj->x += 2; });
incoShow('ref fetch', function () use ($incoObj) { $r = &$incoObj->x; });
incoShow('ref target', function () use ($incoObj) { $y = 5; $incoObj->x =& $y; });
incoShow('subscript base', function () use ($incoObj) { $incoObj->arr[] = 1; });
incoShow('by-ref arg', function () use ($incoObj) { $f = function (&$p) { $p = 7; }; $f($incoObj->x); });

echo "-- ??= reads first, then refuses: BOTH diagnostics, php's order\n";
incoShow('coalesce-assign', fn() => incoCoalAssign($GLOBALS['incoObj']));

echo "-- a method call is its own Error, wherever it is spelled\n";
incoShow('direct call', fn() => $GLOBALS['incoObj']->m());
incoShow('fcc', fn() => $GLOBALS['incoObj']->m(...));
incoShow('method_exists', fn() => method_exists($GLOBALS['incoObj'], 'foo'));
incoShow('is_callable array', fn() => is_callable([$GLOBALS['incoObj'], 'm']));
echo "-- ... but these ask differently and answer without it\n";
incoShow('is_callable obj', fn() => is_callable($GLOBALS['incoObj']));
incoShow('is_callable syntax', fn() => is_callable([$GLOBALS['incoObj'], 'm'], true));
incoShow('call_user_func', fn() => call_user_func([$GLOBALS['incoObj'], 'm']));
incoShow('fromCallable', fn() => Closure::fromCallable([$GLOBALS['incoObj'], 'm']));

echo "-- property_exists probes: warning + false; the string cast names the carrier\n";
incoShow('property_exists', fn() => property_exists($GLOBALS['incoObj'], 'x'));
incoShow('string cast', fn() => (string)$GLOBALS['incoObj']);

echo "-- the untouched payload survives it all\n";
var_dump(serialize($incoObj));
--EXPECT--
-- every READ shape is the access warning answering NULL
  [2] incoRead(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
read => 'was-null'
  [2] incoIsset(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
isset => false
  [2] incoEmpty(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
empty => true
  [2] incoCoal(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
coalesce => 'dflt'
  [2] incoNameM(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
name member => false
  [2] incoByVal(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
by-value arg => 'was-null'
  [2] IncoMeth::go(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
inside a method => 'was-null'
-- every WRITE shape is the modify Error
store !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
unset !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
increment !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
compound !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
ref fetch !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
ref target !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
subscript base !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
by-ref arg !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
-- ??= reads first, then refuses: BOTH diagnostics, php's order
  [2] incoCoalAssign(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
coalesce-assign !! Error: The script tried to modify a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
-- a method call is its own Error, wherever it is spelled
direct call !! Error: The script tried to call a method on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
fcc !! Error: The script tried to call a method on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
method_exists !! Error: The script tried to call a method on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
is_callable array !! Error: The script tried to call a method on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
-- ... but these ask differently and answer without it
is_callable obj => false
is_callable syntax => true
call_user_func !! TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class __PHP_Incomplete_Class does not have a method "m"
fromCallable !! TypeError: Failed to create closure from callable: class __PHP_Incomplete_Class does not have a method "m"
-- property_exists probes: warning + false; the string cast names the carrier
  [2] property_exists(): The script tried to access a property on an incomplete object. Please ensure that the class definition "IncoFoo" of the object you are trying to operate on was loaded _before_ unserialize() gets called or provide an autoloader to load the class definition
property_exists => false
string cast !! Error: Object of class __PHP_Incomplete_Class could not be converted to string
-- the untouched payload survives it all
string(51) "O:7:"IncoFoo":2:{s:1:"x";i:5;s:4:"name";s:3:"abc";}"
