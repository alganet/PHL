--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class php does not early-bind is declared where its statement runs
--DESCRIPTION--
php binds a top-level class at compile time only when it can link it whole: one
implementing an interface, using a trait, an enum, an interface extending
another, a subclass of such a class, or a class whose variance pair needs a
class not loaded yet is declared by its statement, and nothing above it finds
it. It keeps its place in get_declared_classes(), which lists file order.
--FILE--
<?php
var_dump(class_exists('DwrPlain', false), class_exists('DwrPlainKid', false));
var_dump(class_exists('DwrCountable', false), interface_exists('DwrJ', false));
var_dump(class_exists('DwrUsesTrait', false), enum_exists('DwrEnum', false));
var_dump(class_exists('DwrKidOfLate', false), class_exists('DwrOpenPair', false));
try {
    new DwrCountable;
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
var_dump(in_array('DwrCountable', get_declared_classes(), true));
class DwrPlain {}
class DwrPlainKid extends DwrPlain {}
interface DwrI {}
interface DwrJ extends DwrI {}
class DwrCountable implements Countable { function count(): int { return 7; } }
trait DwrT {}
class DwrUsesTrait { use DwrT; }
enum DwrEnum { case A; }
class DwrKidOfLate extends DwrCountable {}
class DwrBase { function f(DwrY $a) {} }
class DwrOpenPair extends DwrBase { function f(DwrX $a) {} }
class DwrX {}
class DwrY extends DwrX {}
var_dump(class_exists('DwrCountable', false), interface_exists('DwrJ', false));
var_dump(class_exists('DwrUsesTrait', false), enum_exists('DwrEnum', false));
var_dump(class_exists('DwrKidOfLate', false), class_exists('DwrOpenPair', false));
var_dump(count(new DwrKidOfLate));
$all = array_values(array_filter(get_declared_classes(), fn($c) => str_starts_with($c, 'Dwr')));
echo implode(",", $all), "\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
Error: Class "DwrCountable" not found
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(7)
DwrPlain,DwrPlainKid,DwrCountable,DwrUsesTrait,DwrEnum,DwrKidOfLate,DwrBase,DwrOpenPair,DwrX,DwrY
