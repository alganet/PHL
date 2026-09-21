--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant and a same-name property coexist (separate namespaces)
--DESCRIPTION--
php keeps class CONSTANTS and PROPERTIES in two separate member namespaces, so `const C`
and `public $C` (or `public static $C`) can be declared together: `C::C` reads the constant,
`$c->C` / `C::$C` reads the property. This exercises both declaration orders, static
properties, inheritance, interface constants, enum constants next to enum cases, and the
member-listing surfaces (reflection getConstants/getProperties, (array) cast, get_class_vars)
which must each keep the two members apart.
--FILE--
<?php
// Both declaration orders.
class A { const C = 1; public $C = 7; }
class B { public $C = 7; const C = 1; }
$a = new A; $b = new B;
echo "A: ", A::C, " ", $a->C, "\n";
echo "B: ", B::C, " ", $b->C, "\n";

// Constant next to a same-name STATIC property. Access the static property by a
// literal name, a variable-variable ($$p), and a braced dynamic name (${$p}) —
// all three are `$`-forms that must resolve to the PROPERTY, while the bareword
// S::C resolves to the CONSTANT.
class S { const C = 1; public static $C = 7; }
$p = "C";
echo "S: ", S::C, " ", S::$C, " ", S::$$p, " ", S::${$p}, "\n";

// Inherited constant next to an own same-name property.
class Base { const C = 10; }
class Child extends Base { public $C = 20; }
$c = new Child;
echo "Child: ", Child::C, " ", $c->C, "\n";

// Interface constant next to a same-name property on the implementer.
interface I { const C = 5; }
class Impl implements I { public $C = 9; }
$i = new Impl;
echo "Impl: ", Impl::C, " ", $i->C, "\n";

// Enum constant next to an enum case.
enum E { case A; const K = 3; }
echo "E: ", E::A->name, " ", E::K, "\n";

// Member-listing surfaces keep them apart.
echo "consts: ", implode(",", array_keys((new ReflectionClass('A'))->getConstants())), "\n";
$props = array_map(fn($p) => $p->getName(), (new ReflectionClass('A'))->getProperties());
echo "props: ", implode(",", $props), "\n";
echo "cast: ", implode(",", array_keys((array)$a)), "\n";
echo "class_vars: ", implode(",", array_keys(get_class_vars('A'))), "\n";
?>
--EXPECT--
A: 1 7
B: 1 7
S: 1 7 7 7
Child: 10 20
Impl: 5 9
E: A 3
consts: C
props: C
cast: C
class_vars: C
