--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
One trait method reaching a class down two paths is not a collision
--DESCRIPTION--
A trait that `use`s another trait flattens it by SHARING the method the origin
trait compiled, so the same method can reach a composing class twice. php tests
the two functions for identity (its own check is on op_array.opcodes) and applies
it once; PHL compared only the two TRAIT names and refused, so
`trait TB { use TA; } class M { use TB, TA; }` -- a valid php program -- was a
compile fatal. Two more answers came out with it: a method the class declares
ITSELF wins over every trait however many of them offer the name (PHL raised a
collision as soon as the second trait arrived), and a property default is
compared by what it MEANS -- a raw memcmp of the compiled default carries the
LINE it was written on and the index of its literal, so the same `= 1` written
in a trait and in the composing class read as incompatible.
--FILE--
<?php
trait TsmtpA { public function m() { return 'A::m'; } }
trait TsmtpB { use TsmtpA; public function b() { return 'b'; } }
trait TsmtpC { use TsmtpA; }
trait TsmtpD { use TsmtpB; }

/* Both paths carry TsmtpA::m. */
class TsmtpOne { use TsmtpB, TsmtpA; }
class TsmtpTwo { use TsmtpB, TsmtpC; }
class TsmtpThree { use TsmtpA, TsmtpB, TsmtpD; }
/* The same trait named twice in one clause. */
class TsmtpFour { use TsmtpA, TsmtpA; }
/* An alias made inside the flattening trait does not fork the method either. */
trait TsmtpE { use TsmtpA { m as aliased; } }
class TsmtpFive { use TsmtpE, TsmtpA; }

foreach (['TsmtpOne', 'TsmtpTwo', 'TsmtpThree', 'TsmtpFour', 'TsmtpFive'] as $tsmtpName) {
    $tsmtpObj = new $tsmtpName();
    echo $tsmtpName, ' => ', $tsmtpObj->m(), "\n";
}
echo 'alias => ', (new TsmtpFive)->aliased(), "\n";

/* A property arriving twice from one origin is not a conflict either. */
trait TsmtpP { public $p = 41; public static $s = 42; }
trait TsmtpQ { use TsmtpP; }
class TsmtpSix { use TsmtpQ, TsmtpP; }
echo 'property => ', (new TsmtpSix)->p, ' ', TsmtpSix::$s, "\n";

/* The concrete definition wins over an abstract requirement, and a class-body
 * method wins over any number of traits offering the name. */
trait TsmtpR { abstract public function m(); }
trait TsmtpS { public function m() { return 'S::m'; } }
class TsmtpSeven { use TsmtpR, TsmtpA; }
class TsmtpEight { use TsmtpA, TsmtpS; public function m() { return 'own'; } }
echo 'abstract => ', (new TsmtpSeven)->m(), "\n";
echo 'class body => ', (new TsmtpEight)->m(), "\n";

/* A default written twice with the same value is one definition, whether the
 * second spelling is another trait's or the composing class's own. */
trait TsmtpT { public $q = 1; public $r = [1, 2]; public $t = 'x'; public $u; }
trait TsmtpU { public $q = 1; }
class TsmtpNine { use TsmtpT, TsmtpU; public $q = 1; public $r = [1, 2]; public $t = 'x'; public $u; }
$tsmtpNine = new TsmtpNine();
echo 'defaults => ', $tsmtpNine->q, ' ', implode('/', $tsmtpNine->r), ' ',
     $tsmtpNine->t, ' ', var_export($tsmtpNine->u, true), "\n";
--EXPECT--
TsmtpOne => A::m
TsmtpTwo => A::m
TsmtpThree => A::m
TsmtpFour => A::m
TsmtpFive => A::m
alias => A::m
property => 41 42
abstract => A::m
class body => own
defaults => 1 1/2 x NULL
