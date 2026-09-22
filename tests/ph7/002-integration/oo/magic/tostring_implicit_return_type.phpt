--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__toString() has an IMPLICIT string return type: an empty string is a value, a scalar coerces, and null/array/object/fall-off raise php's return TypeError (all four used to render the "Object" placeholder)
--DESCRIPTION--
PHL enforced DECLARED return types only, so an undeclared __toString() could
answer anything and MemObjStringValue expanded "Object" for whatever was not a
NON-EMPTY string. Four wrong answers came out of that one condition:
`return ""` printed "Object" instead of nothing, `return 42` printed "Object"
instead of "42", and a null/array/object return printed "Object" where php
raises `C::__toString(): Return value must be of type string, X returned`.

php's rule is that __toString behaves as if declared `: string`, which is also
why declaring any OTHER type is a compile fatal and why reflection reports the
type on an undeclared __toString. Installing the implicit type reuses the
return-type enforcement PHL already matched php on.
--FILE--
<?php
function show(string $label, callable $f): void {
    try {
        $r = $f();
        echo $label, " => ";
        var_dump($r);
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

class Emp  { function __toString() { return ""; } }
class EmpD { function __toString(): string { return ""; } }
class Num  { function __toString() { return 42; } }
class Flt  { function __toString() { return 1.5; } }
class Yes  { function __toString() { return true; } }
class No   { function __toString() { return false; } }
class Nul  { function __toString() { return null; } }
class Arr  { function __toString() { return [1]; } }
class Obj  { function __toString() { return new stdClass(); } }
class Fall { function __toString() { } }

foreach (['Emp', 'EmpD', 'Num', 'Flt', 'Yes', 'No', 'Nul', 'Arr', 'Obj', 'Fall'] as $c) {
    show("cast-$c",    function () use ($c) { return (string)(new $c()); });
    show("interp-$c",  function () use ($c) { $o = new $c(); return "[$o]"; });
    show("concat-$c",  function () use ($c) { return "[" . new $c() . "]"; });
    show("echo-$c",    function () use ($c) { $o = new $c(); echo "<"; echo $o; echo ">"; return 'echoed'; });
    show("implode-$c", function () use ($c) { return implode("|", [new $c(), "z"]); });
    show("strlen-$c",  function () use ($c) { return strlen(new $c()); });
    show("settype-$c", function () use ($c) { $o = new $c(); settype($o, 'string'); return $o; });
}

// The implicit type reaches inherited and trait __toString the same way.
trait TNum { function __toString() { return 7; } }
class UsesT { use TNum; }
abstract class Base { function __toString() { return 9; } }
class Derived extends Base {}
show('trait',     fn () => (string)(new UsesT()));
show('inherited', fn () => (string)(new Derived()));

// Reflection reports the implicit type exactly as php does.
$u = new ReflectionMethod('Emp', '__toString');
var_dump($u->hasReturnType(), (string)$u->getReturnType(), $u->getReturnType()->allowsNull());
$d = new ReflectionMethod('EmpD', '__toString');
var_dump($d->hasReturnType(), (string)$d->getReturnType());

// Stringable is still auto-implemented, and the coercion of a Stringable into a
// declared `string` parameter/return is unchanged.
var_dump(new Emp() instanceof Stringable, new Num() instanceof Stringable);
function takes(string $s): string { return $s; }
var_dump(takes(new Num()));
?>
--EXPECT--
cast-Emp => string(0) ""
interp-Emp => string(2) "[]"
concat-Emp => string(2) "[]"
<>echo-Emp => string(6) "echoed"
implode-Emp => string(2) "|z"
strlen-Emp => int(0)
settype-Emp => string(0) ""
cast-EmpD => string(0) ""
interp-EmpD => string(2) "[]"
concat-EmpD => string(2) "[]"
<>echo-EmpD => string(6) "echoed"
implode-EmpD => string(2) "|z"
strlen-EmpD => int(0)
settype-EmpD => string(0) ""
cast-Num => string(2) "42"
interp-Num => string(4) "[42]"
concat-Num => string(4) "[42]"
<42>echo-Num => string(6) "echoed"
implode-Num => string(4) "42|z"
strlen-Num => int(2)
settype-Num => string(2) "42"
cast-Flt => string(3) "1.5"
interp-Flt => string(5) "[1.5]"
concat-Flt => string(5) "[1.5]"
<1.5>echo-Flt => string(6) "echoed"
implode-Flt => string(5) "1.5|z"
strlen-Flt => int(3)
settype-Flt => string(3) "1.5"
cast-Yes => string(1) "1"
interp-Yes => string(3) "[1]"
concat-Yes => string(3) "[1]"
<1>echo-Yes => string(6) "echoed"
implode-Yes => string(3) "1|z"
strlen-Yes => int(1)
settype-Yes => string(1) "1"
cast-No => string(0) ""
interp-No => string(2) "[]"
concat-No => string(2) "[]"
<>echo-No => string(6) "echoed"
implode-No => string(2) "|z"
strlen-No => int(0)
settype-No => string(0) ""
cast-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
interp-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
concat-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
<echo-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
implode-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
strlen-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
settype-Nul => TypeError: Nul::__toString(): Return value must be of type string, null returned
cast-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
interp-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
concat-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
<echo-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
implode-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
strlen-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
settype-Arr => TypeError: Arr::__toString(): Return value must be of type string, array returned
cast-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
interp-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
concat-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
<echo-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
implode-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
strlen-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
settype-Obj => TypeError: Obj::__toString(): Return value must be of type string, stdClass returned
cast-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
interp-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
concat-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
<echo-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
implode-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
strlen-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
settype-Fall => TypeError: Fall::__toString(): Return value must be of type string, none returned
trait => string(1) "7"
inherited => string(1) "9"
bool(true)
string(6) "string"
bool(false)
bool(true)
string(6) "string"
bool(true)
bool(true)
string(2) "42"
