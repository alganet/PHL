--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Enum reflection (PHP 8.1): isEnum, ReflectionEnum cases/backing, ReflectionEnumUnitCase/BackedCase
--FILE--
<?php
enum EnrSuit: string { case Hearts = "H"; case Spades = "S"; const Wild = "W"; }
enum EnrPure { case Solo; }

$rc = new ReflectionClass("EnrSuit");
echo $rc->isEnum() ? "enum" : "-", " ", $rc->isFinal() ? "final" : "-", " ",
     $rc->isInstantiable() ? "inst" : "noinst", "\n";
echo (new ReflectionClass("stdClass"))->isEnum() ? "enum" : "notenum", "\n";

$re = new ReflectionEnum("EnrSuit");
echo $re->getName(), " ", $re->isBacked() ? "backed" : "pure", " ", (string)$re->getBackingType(), "\n";
echo $re->hasCase("Hearts") ? "has" : "-", $re->hasCase("Wild") ? "?" : "nowild", "\n";
foreach ($re->getCases() as $c) {
    echo get_class($c), " ", $c->getName(), " ", $c->getBackingValue(),
         " ", $c->getValue() === EnrSuit::from($c->getBackingValue()) ? "sing" : "?", "\n";
}
$case = $re->getCase("Spades");
echo $case->isEnumCase() ? "case" : "-", " ", $case->getEnum()->getName(), "\n";
echo (new ReflectionClassConstant("EnrSuit", "Wild"))->isEnumCase() ? "?" : "notcase", "\n";

$rp = new ReflectionEnum("EnrPure");
echo $rp->isBacked() ? "?" : "pure", " ", $rp->getBackingType() === null ? "nulltype" : "?", "\n";
echo get_class($rp->getCases()[0]), "\n";
echo $rp->getCases()[0]->getValue() === EnrPure::Solo ? "solo" : "?", "\n";

try { new ReflectionEnum("stdClass"); }
catch (ReflectionException $e) { echo "ex1: ", $e->getMessage(), "\n"; }
try { new ReflectionEnumUnitCase("EnrSuit", "Wild"); }
catch (ReflectionException $e) { echo "ex2: ", $e->getMessage(), "\n"; }
try { $re->getCase("Missing"); }
catch (ReflectionException $e) { echo "ex3: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
enum final noinst
notenum
EnrSuit backed string
hasnowild
ReflectionEnumBackedCase Hearts H sing
ReflectionEnumBackedCase Spades S sing
case EnrSuit
notcase
pure nulltype
ReflectionEnumUnitCase
solo
ex1: Class "stdClass" is not an enum
ex2: Constant EnrSuit::Wild is not a case
ex3: Case EnrSuit::Missing does not exist
--CLEAN--
<?php
unset($rc, $re, $rp, $case, $c);
