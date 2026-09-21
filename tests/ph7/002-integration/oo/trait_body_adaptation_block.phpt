--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
adaptation blocks (insteadof/as) work inside a TRAIT body and aliases survive composition
--DESCRIPTION--
The trait-body `use` only supported a bare name list; adaptation blocks now
route through the machinery shared with the class body. The alias identity is
the method-table KEY, so an alias made inside a trait survives when that trait
is composed into a class (it used to be silently re-filed under its original
name), and get_class_methods() lists alias names like php.
--FILE--
<?php
trait TbA { public function hi() { return "A::hi"; } public function who() { return "A"; } }
trait TbB { public function hi() { return "B::hi"; } }
trait TbMid {
    use TbA, TbB { TbA::hi insteadof TbB; TbB::hi as bHi; }
}
trait TbMid2 {
    use TbA { hi as protected pHi; who as whoAlias; }
    public function callP() { return $this->pHi(); }
}
class TbUses { use TbMid; }
class TbUses2 { use TbMid2; }
$m = new TbUses;
echo $m->hi(), " ", $m->bHi(), " ", $m->who(), "\n";
$n = new TbUses2;
echo $n->callP(), " ", $n->whoAlias(), "\n";
class TbDirect { use TbA, TbB { TbA::hi insteadof TbB; TbB::hi as bHi; } }
$d = new TbDirect;
echo $d->hi(), " ", $d->bHi(), "\n";
$names = get_class_methods('TbMid');
sort($names);
echo implode(",", $names), "\n";
?>
--EXPECT--
A::hi B::hi A
A::hi A
A::hi B::hi
bHi,hi,who
--CLEAN--
<?php
