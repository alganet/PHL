--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reading a class's statics through reflection materializes the static table
--DESCRIPTION--
The engine's `C::$s` access materializes a class's static table (evaluating a
default whose evaluation was deferred, and raising a typed default's deferred
TypeError), and so do the LIBRARY readers of the same slots:
get_class_vars(), ReflectionClass::getStaticPropertyValue()/
setStaticPropertyValue()/getStaticProperties() and ReflectionProperty::
getValue()/setValue(), plus the instantiating ones (newInstance(),
newInstanceWithoutConstructor(), unserialize()) and a reference BIND to a
static. They read the slot directly, so they used to answer the
NULL a failed default left behind — a silent wrong answer in exactly the API
whose job is to report the class faithfully. Listing without reading —
getProperties(), hasProperty(), property_exists(), class_exists() — does NOT
materialize, in either engine.
--FILE--
<?php
const SdrStr = "abc";

class SdrThrow { public static $s = SDR_UNDEF; public static $t = 1; }
class SdrTyped { public static int $s = SdrStr; }

echo "== get_class_vars ==\n";
try { var_dump(get_class_vars('SdrThrow')); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump(get_class_vars('SdrTyped')); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== ReflectionClass::getStaticPropertyValue (even for the GOOD sibling) ==\n";
try { var_dump((new ReflectionClass('SdrThrow'))->getStaticPropertyValue('t')); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== ReflectionClass::setStaticPropertyValue ==\n";
try { (new ReflectionClass('SdrThrow'))->setStaticPropertyValue('t', 5); echo "set ok\n"; }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== ReflectionClass::getStaticProperties ==\n";
try { var_dump((new ReflectionClass('SdrThrow'))->getStaticProperties()); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== ReflectionProperty::getValue / setValue ==\n";
try { var_dump((new ReflectionProperty('SdrThrow', 't'))->getValue()); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { (new ReflectionProperty('SdrThrow', 't'))->setValue(null, 7); echo "set ok\n"; }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== a name that does not exist still reports the DEFAULT's error ==\n";
try { var_dump((new ReflectionClass('SdrThrow'))->getStaticPropertyValue('nope')); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump((new ReflectionClass('SdrThrow'))->getStaticPropertyValue('nope', 42)); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { (new ReflectionClass('SdrThrow'))->setStaticPropertyValue('nope', 1); echo "set ok\n"; }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== ReflectionProperty::isInitialized ==\n";
try { var_dump((new ReflectionProperty('SdrThrow', 't'))->isInitialized()); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump((new ReflectionProperty('SdrTyped', 's'))->isInitialized()); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== instantiation through reflection and unserialize ==\n";
class SdrNew { public static $s = SDR_UNDEF_N; }
try { var_dump(get_class((new ReflectionClass('SdrNew'))->newInstance())); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
class SdrNoCtor { public static $s = SDR_UNDEF_C; }
try { var_dump(get_class((new ReflectionClass('SdrNoCtor'))->newInstanceWithoutConstructor())); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
class SdrUnser { public static $s = SDR_UNDEF_U; public $p = 1; }
try { var_dump(get_class(unserialize('O:8:"SdrUnser":0:{}'))); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== binding a reference to a static materializes too ==\n";
class SdrRef { public static $s = SDR_UNDEF_R; public static $t = 1; }
$sdrX = 5;
try { SdrRef::$t =& $sdrX; echo "bound\n"; }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== listing without reading does not materialize ==\n";
class SdrList { public static $s = SDR_UNDEF_L; }
var_dump(class_exists('SdrList'), property_exists('SdrList', 's'),
         (new ReflectionClass('SdrList'))->hasProperty('s'),
         count((new ReflectionClass('SdrList'))->getProperties()));

echo "== a class with sound defaults reads normally ==\n";
class SdrOk { public static $a = 1; public static int $b = 2; }
var_dump(get_class_vars('SdrOk'));
var_dump((new ReflectionClass('SdrOk'))->getStaticPropertyValue('b'));
echo "end\n";
?>
--EXPECT--
== get_class_vars ==
Error: Undefined constant "SDR_UNDEF"
TypeError: Cannot assign string to property SdrTyped::$s of type int
== ReflectionClass::getStaticPropertyValue (even for the GOOD sibling) ==
Error: Undefined constant "SDR_UNDEF"
== ReflectionClass::setStaticPropertyValue ==
Error: Undefined constant "SDR_UNDEF"
== ReflectionClass::getStaticProperties ==
Error: Undefined constant "SDR_UNDEF"
== ReflectionProperty::getValue / setValue ==
Error: Undefined constant "SDR_UNDEF"
Error: Undefined constant "SDR_UNDEF"
== a name that does not exist still reports the DEFAULT's error ==
Error: Undefined constant "SDR_UNDEF"
Error: Undefined constant "SDR_UNDEF"
Error: Undefined constant "SDR_UNDEF"
== ReflectionProperty::isInitialized ==
Error: Undefined constant "SDR_UNDEF"
TypeError: Cannot assign string to property SdrTyped::$s of type int
== instantiation through reflection and unserialize ==
Error: Undefined constant "SDR_UNDEF_N"
Error: Undefined constant "SDR_UNDEF_C"
Error: Undefined constant "SDR_UNDEF_U"
== binding a reference to a static materializes too ==
Error: Undefined constant "SDR_UNDEF_R"
== listing without reading does not materialize ==
bool(true)
bool(true)
bool(true)
int(1)
== a class with sound defaults reads normally ==
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
}
int(2)
end
--CLEAN--
<?php
unset($sdrX);
