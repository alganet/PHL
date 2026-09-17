--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A leading backslash (global-namespace anchor) resolves in class-name STRINGS

--FILE--
<?php
// php: a leading '\' anchors a class/interface/trait/enum name to the global
// namespace root; the stored name never carries one, so exactly ONE leading
// backslash is stripped before the lookup. A literal "\\Foo" names a
// non-global "\Foo" that does not exist, so it must NOT resolve.
class T {}
interface I {}
trait Tr {}
enum E {}

// existence probes over the whole family
var_dump(class_exists('\\T'));
var_dump(class_exists('\\Closure'));
var_dump(interface_exists('\\I'));
var_dump(interface_exists('\\Countable'));
var_dump(trait_exists('\\Tr'));
var_dump(enum_exists('\\E'));

// string `new`, instanceof over a string class name, and the not-found message
$c = '\\T';
$o = new $c;
var_dump($o instanceof T);
try { $n = '\\NoSuch'; new $n; } catch (\Throwable $e) { echo $e->getMessage(), "\n"; }

// object/string family through PH7_VmExtractClassFromValue
class A {}
class B extends A {}
var_dump(is_subclass_of('B', '\\A'));
var_dump(is_subclass_of(new B, '\\A'));
var_dump(is_a(new B, '\\A'));
var_dump(method_exists('\\Closure', 'call'));
var_dump(property_exists('\\A', 'x'));

// class_alias anchors both the target and the alias name
class_alias('\\A', '\\Aliased');
var_dump(class_exists('Aliased'));
var_dump(class_exists('\\Aliased'));

// EXACTLY one strip: a doubled leading backslash must stay unresolved
var_dump(class_exists('\\\\T'));
var_dump(method_exists('\\\\Closure', 'call'));

// php strips the anchor, THEN autoloads iff the ORIGINAL name was non-empty,
// passing the STRIPPED (here empty) name: a lone "\" autoloads with "", but a
// truly empty "" does not autoload at all. (Registered last so the earlier
// `new "\NoSuch"` probe does not trip it.)
spl_autoload_register(function ($n) { echo "AUTOLOAD[$n]\n"; });
var_dump(class_exists('\\'));
var_dump(class_exists(''));
var_dump(method_exists('\\', 'm'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Class "\NoSuch" not found
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
AUTOLOAD[]
bool(false)
bool(false)
AUTOLOAD[]
bool(false)
