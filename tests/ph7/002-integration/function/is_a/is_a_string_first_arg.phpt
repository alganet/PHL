--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_a() with a STRING first arg is honored only when allow_string is TRUE

--FILE--
<?php
// php since 5.3.9: is_a($class_name, $class, allow_string: true) resolves a
// string first argument as a class name and applies the instanceof relation
// (the class ITSELF matches, unlike is_subclass_of). allow_string defaults to
// FALSE, so a string first arg without it returns false. An object first arg
// ignores allow_string entirely. Sibling of is_subclass_of()'s string form.
interface I {}
class A implements I {}
class B extends A {}

// string first arg + allow_string=true: parent, self, and interface all match
var_dump(is_a('B', 'A', true));
var_dump(is_a('B', 'B', true));
var_dump(is_a('B', 'I', true));
var_dump(is_a('A', 'B', true));

// allow_string defaults to false -> string first arg is not resolved
var_dump(is_a('B', 'A'));
var_dump(is_a('B', 'A', false));

// leading '\' anchor on either name
var_dump(is_a('\\B', 'A', true));
var_dump(is_a('B', '\\A', true));

// unknown class name resolves to false (no error)
var_dump(is_a('NoSuchClass', 'A', true));

// object first arg: allow_string is ignored, instanceof still includes self
var_dump(is_a(new B, 'A'));
var_dump(is_a(new B, 'B'));
var_dump(is_a(new B, 'I'));
var_dump(is_a(new A, 'B'));
var_dump(is_a(new B, 'A', false));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
