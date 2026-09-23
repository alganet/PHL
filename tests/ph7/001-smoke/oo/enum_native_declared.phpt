--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An enum declared from C is an ordinary enum: case identity, cases()/from()/tryFrom(), reflection
--DESCRIPTION--
PropertyHookType is the engine's own enum, declared from C rather than compiled
from an embedded PHP chunk. An enum is not a class with constants — each case is
a class constant whose slot holds THE singleton instance, materialized lazily —
so the whole of php's enum surface has to hold for one built by the C builder:
`===` identity, the backing value, the two interfaces, match(), and the refusals
(instantiation, cloning, serialization, writing a readonly case property).
--FILE--
<?php
var_dump(PropertyHookType::Get->name, PropertyHookType::Get->value);
var_dump(PropertyHookType::Get === PropertyHookType::Get);
var_dump(PropertyHookType::Get === PropertyHookType::Set);
var_dump(array_map(fn($c) => [$c->name, $c->value], PropertyHookType::cases()));
var_dump(PropertyHookType::from('set')->name);
var_dump(PropertyHookType::tryFrom('get')?->name, PropertyHookType::tryFrom('nope'));
try { PropertyHookType::from('nope'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

var_dump(PropertyHookType::Get instanceof UnitEnum, PropertyHookType::Get instanceof BackedEnum);
var_dump(enum_exists('PropertyHookType'));
echo match (PropertyHookType::Set) {
    PropertyHookType::Get => "matched get\n",
    PropertyHookType::Set => "matched set\n",
};
var_dump(json_encode(PropertyHookType::Get));

$r = new ReflectionEnum('PropertyHookType');
var_dump($r->isEnum(), $r->isBacked(), (string)$r->getBackingType());
var_dump(array_map(fn($c) => [$c->getName(), $c->getBackingValue()], $r->getCases()));
var_dump($r->getCase('Set')->getValue() === PropertyHookType::Set);
var_dump(array_map(fn($p) => [$p->getName(), (string)$p->getType(), $p->isReadOnly()],
                   (new ReflectionClass('PropertyHookType'))->getProperties()));
var_dump(array_keys((new ReflectionClass('PropertyHookType'))->getConstants()));

// An enum serializes to php's own case reference, not to its properties.
var_dump(serialize(PropertyHookType::Get));
var_dump(unserialize(serialize(PropertyHookType::Set)) === PropertyHookType::Set);

// The refusals php makes for any enum.
try { new PropertyHookType(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $c = clone PropertyHookType::Get; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$case = PropertyHookType::Get;
try { $case->value = 'x'; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
string(3) "Get"
string(3) "get"
bool(true)
bool(false)
array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(3) "Get"
    [1]=>
    string(3) "get"
  }
  [1]=>
  array(2) {
    [0]=>
    string(3) "Set"
    [1]=>
    string(3) "set"
  }
}
string(3) "Set"
string(3) "Get"
NULL
ValueError: "nope" is not a valid backing value for enum PropertyHookType
bool(true)
bool(true)
bool(true)
matched set
string(5) ""get""
bool(true)
bool(true)
string(6) "string"
array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(3) "Get"
    [1]=>
    string(3) "get"
  }
  [1]=>
  array(2) {
    [0]=>
    string(3) "Set"
    [1]=>
    string(3) "set"
  }
}
bool(true)
array(2) {
  [0]=>
  array(3) {
    [0]=>
    string(4) "name"
    [1]=>
    string(6) "string"
    [2]=>
    bool(true)
  }
  [1]=>
  array(3) {
    [0]=>
    string(5) "value"
    [1]=>
    string(6) "string"
    [2]=>
    bool(true)
  }
}
array(2) {
  [0]=>
  string(3) "Get"
  [1]=>
  string(3) "Set"
}
string(28) "E:20:"PropertyHookType:Get";"
bool(true)
Error: Cannot instantiate enum PropertyHookType
Error: Trying to clone an uncloneable object of class PropertyHookType
Error: Cannot modify readonly property PropertyHookType::$value
