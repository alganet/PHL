--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Set visibilities php accepts on a redeclared property
--DESCRIPTION--
A child may add a set visibility no narrower than its parent's read visibility
or its explicit set one, and any under a get-only virtual parent, which has no
set access at all. A set visibility equal to the read one is dropped, so
`private private(set)` is neither private(set) nor final, and a subclass may
redeclare it.
--FILE--
<?php
class PsaProt { protected int $a = 1; }
class PsaProtKid extends PsaProt { public protected(set) int $a = 2; }
class PsaPset { public protected(set) int $b = 1; }
class PsaPsetKid extends PsaPset { public protected(set) int $b = 3; }
class PsaPsetWide extends PsaPset { public int $b = 4; }
class PsaRo { public readonly int $c; }
class PsaRoKid extends PsaRo { public protected(set) readonly int $c; }
class PsaGet { public int $d { get => 5; } }
class PsaGetKid extends PsaGet { public private(set) int $d = 6; }
interface PsaI { public int $e { get; } }
class PsaImpl implements PsaI { public private(set) int $e = 7; }
class PsaPriv { private private(set) int $f = 8; }
class PsaPrivKid extends PsaPriv { public int $f = 9; }
class PsaSame { protected protected(set) int $g = 10; }
class PsaSameKid extends PsaSame { public int $g = 11; }
var_dump((new PsaProtKid)->a, (new PsaPsetKid)->b, (new PsaPsetWide)->b, (new PsaGetKid)->d,
    (new PsaImpl)->e, (new PsaPrivKid)->f, (new PsaSameKid)->g);
foreach (['f' => 'PsaPriv', 'g' => 'PsaSame'] as $n => $c) {
    $r = new ReflectionProperty($c, $n);
    var_dump($r->isFinal(), $r->isPrivateSet(), $r->isProtectedSet());
    echo $r;
}
?>
--EXPECT--
int(2)
int(3)
int(4)
int(5)
int(7)
int(9)
int(11)
bool(false)
bool(false)
bool(false)
Property [ private int $f = 8 ]
bool(false)
bool(false)
bool(false)
Property [ protected int $g = 10 ]
