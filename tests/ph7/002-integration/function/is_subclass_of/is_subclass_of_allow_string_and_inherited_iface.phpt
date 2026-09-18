--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_subclass_of() honors allow_string (default TRUE) and finds inherited interfaces

--FILE--
<?php
// php: is_subclass_of($object_or_class, $class, allow_string: true) — the SUBCLASS
// relation, which EXCLUDES the class itself (unlike is_a). allow_string defaults to
// TRUE here (unlike is_a's FALSE), so a string first arg IS resolved unless the flag
// is explicitly false. An object first arg ignores the flag. Crucially, an interface
// implemented by a PARENT class must be found (B extends A implements I).
interface Base {}
interface I extends Base {}
class A implements I {}
class B extends A {}
class C extends B {}

// string first arg: subclass, self (excluded), and INHERITED interfaces
var_dump(is_subclass_of('B', 'A'));
var_dump(is_subclass_of('C', 'A'));
var_dump(is_subclass_of('B', 'B'));
var_dump(is_subclass_of('B', 'I'));
var_dump(is_subclass_of('B', 'Base'));
var_dump(is_subclass_of('C', 'I'));

// allow_string defaults to TRUE; only an explicit false suppresses the string form
var_dump(is_subclass_of('B', 'A', true));
var_dump(is_subclass_of('B', 'A', false));
var_dump(is_subclass_of('B', 'I', false));

// leading '\' anchor and unknown class
var_dump(is_subclass_of('\\B', 'A'));
var_dump(is_subclass_of('NoSuchClass', 'A'));

// object first arg: allow_string is ignored, inherited interface still found
var_dump(is_subclass_of(new B, 'A'));
var_dump(is_subclass_of(new B, 'A', false));
var_dump(is_subclass_of(new B, 'B'));
var_dump(is_subclass_of(new B, 'I'));
var_dump(is_subclass_of(new C, 'Base', false));

// non-string / non-object first arg
var_dump(is_subclass_of(5, 'A'));
var_dump(is_subclass_of(null, 'A', false));
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
