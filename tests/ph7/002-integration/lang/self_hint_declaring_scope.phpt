--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self/parent in a type hint resolve against the DECLARING class, not the runtime one
--FILE--
<?php
class Base {}
class Oth {}

class P extends Base {
    public self $prop;
    public self|false $uprop = false;
    public parent $par;

    public function setProp($v) { $this->prop = $v; }
    public function setUnion($v) { $this->uprop = $v; }
    public function setPar($v) { $this->par = $v; }

    public function ret(): self { return new P; }
    public function retPar(): parent { return new Base; }
    public function retUnion(): self|false { return new P; }
    public function bad(): self { return new Oth; }
}
class Q extends P {}

trait T {
    public function tret(): self { return new self; }
}
class UsesT { use T; }

$show = function (callable $fn) {
    try { $fn(); echo "ok\n"; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
};

// Reached through a SUBCLASS instance, a hint written in P still means P.
$q = new Q;
$show(fn() => var_dump(get_class($q->ret())));
$show(fn() => var_dump(get_class($q->retPar())));
$show(fn() => var_dump(get_class($q->retUnion())));
$show(fn() => $q->setProp(new P));
$show(fn() => $q->setUnion(new P));
$show(fn() => $q->setPar(new Base));

// The check is still a real check: an unrelated class is rejected either way.
$show(fn() => $q->bad());
$show(fn() => $q->setProp(new Oth));
$show(fn() => $q->setUnion(new Oth));

// A trait's `self` is the USING class (php flattens the trait in).
$u = new UsesT;
$show(fn() => var_dump(get_class($u->tret())));
?>
--EXPECT--
string(1) "P"
ok
string(4) "Base"
ok
string(1) "P"
ok
ok
ok
ok
P::bad(): Return value must be of type P, Oth returned
Cannot assign Oth to property P::$prop of type P
Cannot assign Oth to property P::$uprop of type P|false
string(5) "UsesT"
ok
--CLEAN--
<?php
