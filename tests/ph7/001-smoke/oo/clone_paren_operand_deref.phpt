--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone (expr) followed by ->/[]/()/:: clones what the dereference yields
--FILE--
<?php
class CloneDerefA {
    public $n = 1;
    public function __clone() { echo "[cloneA]"; }
    public function make() { echo "[make]"; return new CloneDerefB; }
    public function __invoke() { echo "[invoke]"; return new CloneDerefB; }
}
class CloneDerefB {
    public $m = 2;
    public function __clone() { echo "[cloneB]"; }
}
class CloneDerefBag implements ArrayAccess {
    public function __clone() { echo "[cloneBag]"; }
    public function offsetExists($o): bool { return true; }
    public function offsetGet($o): mixed { echo "[get]"; return new CloneDerefB; }
    public function offsetSet($o, $v): void {}
    public function offsetUnset($o): void {}
}

$a = new CloneDerefA;
$bag = new CloneDerefBag;

// -> : the operand is the whole postfix chain, so the RESULT of make() is cloned
$r = clone (new CloneDerefA)->make();
echo " => ", get_class($r), "\n";
$r = clone ($a)->make();
echo " => ", get_class($r), "\n";
$r = clone ($a)?->make();
echo " => ", get_class($r), "\n";

// [] : the ELEMENT is cloned, not the container
$r = clone ($bag)[0];
echo " => ", get_class($r), "\n";

// () : the call's return value is cloned
$r = clone ($a)();
echo " => ", get_class($r), "\n";

// :: yields a string here, which is not cloneable
try { $r = clone ($a)::class; } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }

// nothing dereferences the group: the one-argument clone() call form
$r = clone ($a);
echo " => ", get_class($r), " n=", $r->n, "\n";
$r = clone($a, ['n' => 7]);
echo " => ", get_class($r), " n=", $r->n, "\n";

// the operator form over an unparenthesised chain is unchanged
$r = clone $a->make();
echo " => ", get_class($r), "\n";
?>
--EXPECT--
[make][cloneB] => CloneDerefB
[make][cloneB] => CloneDerefB
[make][cloneB] => CloneDerefB
[get][cloneB] => CloneDerefB
[invoke][cloneB] => CloneDerefB
clone(): Argument #1 ($object) must be of type object, string given
[cloneA] => CloneDerefA n=1
[cloneA] => CloneDerefA n=7
[make][cloneB] => CloneDerefB
