--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Enums (PHP 8.1): catchable runtime errors — new/clone/from/tryFrom/readonly/dup-value/type-mismatch
--FILE--
<?php
enum EneColor: string { case Red = "r"; case Blue = "b"; }
enum EnePure { case One; }
enum EneDup: int { case A = 1; case B = 1; }
enum EneBad: int { case Broken = "zzz"; case Fine = 5; }
class EneRef { const FINE = EneBad::Fine; } // constant-expression reference: evaluates ONLY that case

function ene_try(callable $f): void {
    try { $f(); echo "no-throw\n"; }
    catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
ene_try(fn() => new EneColor());
ene_try(fn() => clone EneColor::Red);
ene_try(fn() => EneColor::from("nope"));
ene_try(fn() => EneColor::tryFrom(4)); // weak mode coerces int 4 to "4": a miss, null, no throw
ene_try(function() { $o = EneColor::Red; $o->name = "X"; });
ene_try(fn() => EneDup::B); // direct access evaluates every case: duplicate detected
ene_try(fn() => EneBad::Broken);
echo EneRef::FINE->name, "\n"; // ...but the constant-expression path reaches the valid sibling
echo EneColor::tryFrom("nope") === null ? "trynull" : "?", "\n";
echo EnePure::One->name, "\n";
?>
--EXPECT--
Error: Cannot instantiate enum EneColor
Error: Trying to clone an uncloneable object of class EneColor
ValueError: "nope" is not a valid backing value for enum EneColor
no-throw
Error: Cannot modify readonly property EneColor::$name
Error: Duplicate value in enum EneDup for cases A and B
TypeError: Enum case type string does not match enum backing type int
Fine
trynull
One
--CLEAN--
<?php
unset($o);
