--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a trait composition is inherited whole — insteadof choice and `as` aliases
--FILE--
<?php
trait TciA {
    public function m() { return 'A::m'; }
    public function n() { return 'A::n'; }
}
trait TciB {
    public function m() { return 'B::m'; }
    public function n() { return 'B::n'; }
}
class TciBase {
    use TciA, TciB {
        TciA::m insteadof TciB;
        TciB::n insteadof TciA;
        TciB::m as mFromB;
        TciA::n as protected nFromA;
    }
}
class TciKid extends TciBase {
    public function reach() { return $this->nFromA(); }
}
class TciGrand extends TciKid {}

$b = new TciBase;
echo $b->m(), ' ', $b->n(), ' ', $b->mFromB(), "\n";

// The subclass answers exactly as the composing class does: the insteadof
// choice picks the same body, and the alias is a name the child has too.
$k = new TciKid;
echo $k->m(), ' ', $k->n(), ' ', $k->mFromB(), ' ', $k->reach(), "\n";

// ...and it survives a second level of inheritance.
$g = new TciGrand;
echo $g->m(), ' ', $g->n(), ' ', $g->mFromB(), "\n";

// A child may override the inherited ALIAS; the original stays put.
class TciOwn extends TciBase {
    public function mFromB() { return 'own'; }
}
$o = new TciOwn;
echo $o->mFromB(), ' ', $o->m(), "\n";

var_dump(method_exists('TciKid', 'mFromB'), is_callable([new TciKid, 'mFromB']));
?>
--EXPECT--
A::m B::n B::m
A::m B::n B::m A::n
A::m B::n B::m
own A::m
bool(true)
bool(true)
--CLEAN--
<?php
