--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the expected-type TEXT resolves self/parent/static in every message shape
--FILE--
<?php
class Oth {}
class Base {}

class P extends Base {
    public self $ps;
    public ?self $pns;
    public self|false $puf;
    public parent|int $ppi;

    public function setPs($v) { $this->ps = $v; }
    public function setPns($v) { $this->pns = $v; }
    public function setPuf($v) { $this->puf = $v; }
    public function setPpi($v) { $this->ppi = $v; }

    public function argSelf(self|false $x) {}
    public function argParent(parent|int $x) {}
    public function varSelf(self|false ...$x) {}
    public function retSelf(): self|int { return new Oth; }
    public function retStatic(): static|false { return new Oth; }
    public function retNullable(): ?self { return new Oth; }
}
class Q extends P {}

$show = function (callable $fn) {
    try { $fn(); echo "ok\n"; }
    catch (TypeError $e) {
        $m = $e->getMessage();
        $cut = strpos($m, ', called in');
        echo $cut === false ? $m : substr($m, 0, $cut), "\n";
    }
};

// Reached through a SUBCLASS: `self`/`parent` still name where they were
// written, `static` names the class the call was made through.
$q = new Q;
$show(function () use ($q) { $q->setPs(new Oth); });
$show(function () use ($q) { $q->setPns(new Oth); });
$show(function () use ($q) { $q->setPuf(new Oth); });
$show(function () use ($q) { $q->setPpi(new Oth); });
$show(fn() => $q->argSelf(new Oth));
$show(fn() => $q->argParent(new Oth));
$show(fn() => $q->varSelf(new Oth));
$show(fn() => $q->retSelf());
$show(fn() => $q->retStatic());
$show(fn() => $q->retNullable());
?>
--EXPECT--
Cannot assign Oth to property P::$ps of type P
Cannot assign Oth to property P::$pns of type ?P
Cannot assign Oth to property P::$puf of type P|false
Cannot assign Oth to property P::$ppi of type Base|int
P::argSelf(): Argument #1 ($x) must be of type P|false, Oth given
P::argParent(): Argument #1 ($x) must be of type Base|int, Oth given
P::varSelf(): Argument #1 must be of type P|false, Oth given
P::retSelf(): Return value must be of type P|int, Oth returned
P::retStatic(): Return value must be of type Q|false, Oth returned
P::retNullable(): Return value must be of type ?P, Oth returned
--CLEAN--
<?php
