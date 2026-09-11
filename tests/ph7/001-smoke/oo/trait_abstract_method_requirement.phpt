--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an abstract trait method is a requirement satisfied by a concrete one, not a collision
--FILE--
<?php
/* php treats an abstract method declared in a trait as a REQUIREMENT: any concrete
 * method of the same name — from the class body or another trait — satisfies it
 * with no conflict. PHL wrongly flagged abstract-vs-concrete as an unresolved trait
 * collision, refused to apply the concrete method, then declared the whole class
 * abstract, which cascaded into "Call to undefined function" for UNRELATED class
 * methods. Regression for PHPUnit's mock traits (the Method trait declares
 * `abstract __phpunit_getInvocationHandler()`, StubApi provides it). */
trait NtaRequires {
    abstract public function handler(): string;
    public function useHandler(): string { return $this->handler() . "!"; }
}
trait NtaProvides {
    public function handler(): string { return "H"; }
}
/* concrete-from-trait satisfies the abstract, in BOTH use orders */
class NtaA { use NtaProvides; use NtaRequires; public function ping(): string { return "a"; } }
class NtaB { use NtaRequires; use NtaProvides; public function ping(): string { return "b"; } }
/* concrete-from-class-body satisfies the abstract */
class NtaC { use NtaRequires; public function handler(): string { return "C"; } public function ping(): string { return "c"; } }

foreach (['NtaA', 'NtaB', 'NtaC'] as $c) {
    $o = new $c;
    echo $c, ": ", $o->ping(), " ", $o->handler(), " ", $o->useHandler(), "\n";
}
?>
--EXPECT--
NtaA: a H H!
NtaB: b H H!
NtaC: c C C!
--CLEAN--
<?php
