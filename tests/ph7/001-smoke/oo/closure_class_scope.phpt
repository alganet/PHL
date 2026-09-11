--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a closure/arrow-fn defined in a class carries that class as its scope, so it may touch private/protected members
--FILE--
<?php
/* php binds a closure's scope to its defining class: `$this->privateMethod()`,
 * `self::CONST`, and a private member of ANY same-class instance passed in are
 * all reachable from inside the closure body. PHL stamped the creation-site
 * class on the closure (for self:: resolution) but the visibility check ignored
 * it for VM_FUNC_CLOSURE frames, so every such access raised "from global
 * scope". Regression for PHPUnit's Operator constraints
 * (`array_map(fn($c) => $this->checkConstraint($c), $constraints)`). */
class NcsBase { const K = 7; }
class NcsScope extends NcsBase {
    private $secret = "S";
    private function priv() { return "P"; }
    protected function prot() { return "R"; }
    /* arrow fn capturing $this */
    public function aThis() { $f = fn () => $this->priv() . $this->secret; return $f(); }
    /* self:: inside the arrow */
    public function aSelf() { $f = fn () => self::K; return $f(); }
    /* long-form closure capturing $this, protected method */
    public function cThis() { $f = function () { return $this->prot(); }; return $f(); }
    /* via array_map callback (the PHPUnit shape) */
    public function mapped(array $xs) {
        return array_map(fn ($x) => $this->twice($x), $xs);
    }
    private function twice($x) { return $x * 2; }
    /* passed same-class instance (not $this): scope still grants access */
    public function passed(NcsScope $o) { $f = fn () => $o->priv(); return $f(); }
    /* static arrow reaching a passed object's private */
    public function staticPassed(NcsScope $o) { $f = static fn () => $o->priv(); return $f(); }
}
$o = new NcsScope();
echo $o->aThis(), "\n";                       // PS
echo $o->aSelf(), "\n";                        // 7
echo $o->cThis(), "\n";                        // R
echo implode(",", $o->mapped([1, 2, 3])), "\n"; // 2,4,6
echo $o->passed(new NcsScope()), "\n";          // P
echo $o->staticPassed(new NcsScope()), "\n";    // P

/* A closure defined OUTSIDE the class gets no such scope. */
$outsider = fn ($x) => $x->priv();
try { $outsider($o); echo "LEAK\n"; }
catch (\Error $e) { echo "denied\n"; }
?>
--EXPECT--
PS
7
R
2,4,6
P
P
denied
--CLEAN--
<?php
