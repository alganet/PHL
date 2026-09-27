--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's marker attribute classes and SensitiveParameterValue are declared from C
--DESCRIPTION--
Three of php's attribute classes declare nothing but their own target mask,
because what they MEAN is a question something else asks: `AllowDynamicProperties`
is read by the dynamic-property decision at the write site, `SensitiveParameter`
by the backtrace builder, `ReturnTypeWillChange` by php's tentative-return-type
check. None of them existed here, so a program that spelled one and then asked
`getAttributes()[0]->newInstance()` got `Attribute class "..." not found` instead
of php's object. `SensitiveParameterValue` is the box php puts a redacted
argument in: it holds a `private readonly mixed $value`, answers `getValue()`,
and shows NOTHING to any display surface (php gives it a get_properties handler
that answers null for every purpose, not just to var_dump's `__debugInfo()`)
while refusing serialization in both directions.
--FILE--
<?php
function markerShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

echo "-- each marker carries php's own mask\n";
foreach (['AllowDynamicProperties', 'SensitiveParameter', 'ReturnTypeWillChange'] as $c) {
    $r = new ReflectionClass($c);
    echo '  ', $c, ': final=', var_export($r->isFinal(), true),
        ' internal=', var_export($r->isInternal(), true),
        ' props=', count($r->getProperties()),
        ' mask=', json_encode($r->getAttributes()[0]->getArguments()), "\n";
}

echo "-- and an argless constructor php also declares\n";
markerShow('argless', fn() => get_class(new AllowDynamicProperties()));
markerShow('one too many', fn() => new SensitiveParameter(1));

echo "-- they reach newInstance() where they are written\n";
#[\AllowDynamicProperties]
class MarkerDyn {}
function markerLogin(string $user, #[\SensitiveParameter] string $pass) { return $user; }
class MarkerIter implements Iterator {
    #[\ReturnTypeWillChange]
    public function current() { return 1; }
    public function key(): mixed { return 0; }
    public function next(): void {}
    public function rewind(): void {}
    public function valid(): bool { return false; }
}
markerShow('on a class', fn() => get_class(
    (new ReflectionClass('MarkerDyn'))->getAttributes()[0]->newInstance()));
markerShow('on a parameter', fn() => get_class(
    (new ReflectionFunction('markerLogin'))->getParameters()[1]->getAttributes()[0]->newInstance()));
markerShow('on a method', fn() => get_class(
    (new ReflectionMethod('MarkerIter', 'current'))->getAttributes()[0]->newInstance()));
markerShow('the class still allows a dynamic property', function () {
    $o = new MarkerDyn();
    $o->late = 7;
    return $o->late;
});

echo "-- SensitiveParameterValue is the box, and it shows nothing\n";
markerShow('value', fn() => (new SensitiveParameterValue('s3cret'))->getValue());
markerShow('array value', fn() => (new SensitiveParameterValue(['k' => 1]))->getValue());
markerShow('debugInfo', fn() => (new SensitiveParameterValue('s3cret'))->__debugInfo());
markerShow('cast', fn() => (array)new SensitiveParameterValue('s3cret'));
markerShow('json', fn() => json_encode(new SensitiveParameterValue('s3cret')));
markerShow('export', fn() => var_export(new SensitiveParameterValue('s3cret'), true));
markerShow('vars', fn() => get_object_vars(new SensitiveParameterValue('s3cret')));
markerShow('the slot is private', function () {
    $v = new SensitiveParameterValue('s3cret');
    return $v->value;
});
markerShow('slot modifiers', fn() => implode(' ', Reflection::getModifierNames(
    (new ReflectionProperty('SensitiveParameterValue', 'value'))->getModifiers())));
markerShow('slot type', fn() => (string)(new ReflectionProperty('SensitiveParameterValue', 'value'))->getType());
markerShow('argless', fn() => new SensitiveParameterValue());
markerShow('serialize', fn() => serialize(new SensitiveParameterValue('s3cret')));
markerShow('unserialize', fn() => unserialize('O:23:"SensitiveParameterValue":0:{}'));
markerShow('clone keeps it', fn() => (clone new SensitiveParameterValue('s3cret'))->getValue());
--EXPECT--
-- each marker carries php's own mask
  AllowDynamicProperties: final=true internal=true props=0 mask=[1]
  SensitiveParameter: final=true internal=true props=0 mask=[32]
  ReturnTypeWillChange: final=true internal=true props=0 mask=[4]
-- and an argless constructor php also declares
argless => 'AllowDynamicProperties'
one too many => ArgumentCountError: SensitiveParameter::__construct() expects exactly 0 arguments, 1 given
-- they reach newInstance() where they are written
on a class => 'AllowDynamicProperties'
on a parameter => 'SensitiveParameter'
on a method => 'ReturnTypeWillChange'
the class still allows a dynamic property => 7
-- SensitiveParameterValue is the box, and it shows nothing
value => 's3cret'
array value => array (  'k' => 1,)
debugInfo => array ()
cast => array ()
json => '{}'
export => '\\SensitiveParameterValue::__set_state(array())'
vars => array ()
the slot is private => Error: Cannot access private property SensitiveParameterValue::$value
slot modifiers => 'private readonly'
slot type => 'mixed'
argless => ArgumentCountError: SensitiveParameterValue::__construct() expects exactly 1 argument, 0 given
serialize => Exception: Serialization of 'SensitiveParameterValue' is not allowed
unserialize => Exception: Unserialization of 'SensitiveParameterValue' is not allowed
clone keeps it => 's3cret'
