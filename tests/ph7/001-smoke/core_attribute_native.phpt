--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Attribute and Deprecated are declared from C, carrying their own #[Attribute]
--DESCRIPTION--
php's two attribute classes each carry an ATTRIBUTE of their own, and the record
is load-bearing rather than decorative: it is what the engine reads to decide
whether a user's `#[Deprecated]` may sit where it does. A class declared from C
has no compiler to emit the argument's byte-code, so the value rides as a
literal — the last piece of native-class machinery this workstream needed. The
declarations were wrong twice over besides: php's Deprecated mask is 87 (it
includes TARGET_CLASS, which php 8.5 uses to mark a deprecated TRAIT and refuses
by name for every other class kind), and php declares `public int $flags;` with
no default plus two `public protected(set) readonly ?string` slots.
--FILE--
<?php
function attrShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

echo "-- each class carries its own #[Attribute], with php's mask\n";
foreach (['Attribute', 'Deprecated'] as $c) {
    foreach ((new ReflectionClass($c))->getAttributes() as $a) {
        echo '  ', $c, ': ', $a->getName(), ' ', json_encode($a->getArguments()),
            ' -> flags=', $a->newInstance()->flags, "\n";
    }
}

echo "-- the declarations\n";
attrShow('Attribute final', fn() => (new ReflectionClass('Attribute'))->isFinal());
attrShow('flags has no default', fn() => (new ReflectionProperty('Attribute', 'flags'))->hasDefaultValue());
attrShow('flags type', fn() => (string)(new ReflectionProperty('Attribute', 'flags'))->getType());
attrShow('ctor default', fn() => (new ReflectionParameter(['Attribute', '__construct'], 'flags'))->getDefaultValue());
attrShow('constants', fn() => implode(',', array_keys((new ReflectionClass('Attribute'))->getConstants())));
attrShow('message modifiers', fn() => implode(' ', Reflection::getModifierNames(
    (new ReflectionProperty('Deprecated', 'message'))->getModifiers())));
attrShow('message type', fn() => (string)(new ReflectionProperty('Deprecated', 'message'))->getType());

echo "-- instances\n";
attrShow('default flags', fn() => (new Attribute)->flags);
attrShow('given flags', fn() => (new Attribute(Attribute::TARGET_PROPERTY))->flags);
attrShow('deprecated pair', fn() => (new Deprecated('gone', '8.4'))->message
    . '/' . (new Deprecated('gone', '8.4'))->since);
attrShow('deprecated empty', fn() => (new Deprecated)->message);
attrShow('deprecated is readonly', function () { $d = new Deprecated('m'); $d->message = 'x'; return 'written'; });

echo "-- a user attribute still validates against its own mask\n";
#[Attribute(Attribute::TARGET_METHOD)]
class AttrOnlyMethod {}
class AttrHolder {
    #[AttrOnlyMethod]
    public function m() {}
    #[AttrOnlyMethod]
    public $p;
}
attrShow('on a method', fn() => get_class(
    (new ReflectionMethod('AttrHolder', 'm'))->getAttributes()[0]->newInstance()));
attrShow('on a property', fn() => get_class(
    (new ReflectionProperty('AttrHolder', 'p'))->getAttributes()[0]->newInstance()));
--EXPECT--
-- each class carries its own #[Attribute], with php's mask
  Attribute: Attribute [1] -> flags=1
  Deprecated: Attribute [87] -> flags=87
-- the declarations
Attribute final => true
flags has no default => false
flags type => 'int'
ctor default => 127
constants => 'TARGET_CLASS,TARGET_FUNCTION,TARGET_METHOD,TARGET_PROPERTY,TARGET_CLASS_CONSTANT,TARGET_PARAMETER,TARGET_CONSTANT,TARGET_ALL,IS_REPEATABLE'
message modifiers => 'public protected(set) readonly'
message type => '?string'
-- instances
default flags => 127
given flags => 8
deprecated pair => 'gone/8.4'
deprecated empty => NULL
deprecated is readonly => Error: Cannot modify readonly property Deprecated::$message
-- a user attribute still validates against its own mask
on a method => 'AttrOnlyMethod'
on a property => Error: Attribute "AttrOnlyMethod" cannot target property (allowed targets: method)
