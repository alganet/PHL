--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Parent-class private properties live on child instances (base methods read/write them; child scope and outsiders are denied)
--FILE--
<?php
class PppB {
    private $bp = 9;
    function readOwn() { return $this->bp; }
    function setOwn($v) { $this->bp = $v; }
}
class PppC extends PppB {
    function tryChild() { return $this->bp ?? "child-denied"; }
}
$pppC = new PppC();
echo "read: ", $pppC->readOwn(), "\n";
$pppC->setOwn(42);
echo "after set: ", $pppC->readOwn(), "\n";
echo "child scope: ", $pppC->tryChild(), "\n";
echo "outside: ", $pppC->bp ?? "outside-denied", "\n";
echo "isset outside: ", isset($pppC->bp) ? "T" : "F", "\n";
var_dump($pppC);
echo "own-class: ", (new PppB())->readOwn(), "\n";
trait PppT { private $tp = 5; function readTrait() { return $this->tp; } }
class PppU { use PppT; function readAdopted() { return $this->tp; } }
$pppU = new PppU();
echo "trait: ", $pppU->readTrait(), "/", $pppU->readAdopted(), "\n";
class PppG extends PppC { }
echo "grandchild: ", (new PppG())->readOwn(), "\n";
// get_object_vars sees the base private only from base scope
class PppScope extends PppB { function varsFromChild() { return array_keys(get_object_vars($this)); } }
echo json_encode((new PppScope())->varsFromChild()), "\n";
?>
--EXPECTF--
read: 9
after set: 42
child scope: child-denied
outside: outside-denied
isset outside: F
object(PppC)#%d (1) {
  ["bp":"PppB":private]=>
  int(42)
}
own-class: 9
trait: 5/5
grandchild: 9
[]
