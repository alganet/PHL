--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
TypeError text for a class hint: the ? of a nullable, iterable's alternatives, the declaring class
--FILE--
<?php
class Cee {}
class Oth {}
interface Iface {}

function nullableClass(?Cee $c) {}
function nullableIface(?Iface $i) {}
function nullableIterable(?iterable $i) {}
function plainIterable(iterable $i) {}
function nullableReturn(): ?Cee { return new Oth; }
function variadicNullable(?Cee ...$c) {}

class Holder {
    public function self_(?self $x) {}
    public function ret(): ?self { return new Oth; }
    public function scalar(): int { return 'nope'; }
    public function never_(): never {}
}
class Sub extends Holder {}

$show = function (callable $fn) {
    try { $fn(); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
};

// A nullable CLASS hint keeps its `?` — it used to print the bare class name.
$show(fn() => nullableClass(new Oth));
$show(fn() => nullableIface(new Oth));
$show(fn() => variadicNullable(new Oth));
$show(fn() => nullableReturn());
$show(fn() => (new Holder)->self_(new Oth));      // ?self names the class it stands for
$show(fn() => (new Holder)->ret());

// `iterable` is php's alias for Traversable|array and prints that way.
$show(fn() => plainIterable(new Oth));
$show(fn() => nullableIterable(new Oth));

// A return TypeError names the DECLARING class of a method, like php.
$show(fn() => (new Sub)->scalar());
$show(fn() => (new Sub)->never_());
?>
--EXPECTF--
nullableClass(): Argument #1 ($c) must be of type ?Cee, Oth given, called in %s on line %d
nullableIface(): Argument #1 ($i) must be of type ?Iface, Oth given, called in %s on line %d
variadicNullable(): Argument #1 must be of type ?Cee, Oth given, called in %s on line %d
nullableReturn(): Return value must be of type ?Cee, Oth returned
Holder::self_(): Argument #1 ($x) must be of type ?Holder, Oth given, called in %s on line %d
Holder::ret(): Return value must be of type ?Holder, Oth returned
plainIterable(): Argument #1 ($i) must be of type Traversable|array, Oth given, called in %s on line %d
nullableIterable(): Argument #1 ($i) must be of type Traversable|array|null, Oth given, called in %s on line %d
Holder::scalar(): Return value must be of type int, string returned
Holder::never_(): never-returning method must not implicitly return
--CLEAN--
<?php
