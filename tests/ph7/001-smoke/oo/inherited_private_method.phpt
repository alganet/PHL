--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: inherited methods reach the base class's private methods on child instances
--FILE--
<?php
class IpmA {
    private function m() { return 'p'; }
    public function c() { return $this->m(); }
}
class IpmB extends IpmA {}
echo (new IpmB)->c(), "\n";

trait IpmT {
    private function tm() { return 't'; }
}
class IpmC {
    use IpmT;
    public function c() { return $this->tm(); }
}
class IpmD extends IpmC {}
echo (new IpmD)->c(), "\n";

class IpmE {
    protected function p() { return 'pr'; }
    public function c() { return $this->p(); }
}
class IpmF extends IpmE {}
echo (new IpmF)->c(), "\n";

// php binds private calls by declaring class: the child's private shadow
// does not hijack the parent's own call site, and vice versa
class IpmG {
    private function s() { return 'base'; }
    public function c() { return $this->s(); }
}
class IpmH extends IpmG {
    private function s() { return 'child'; }
    public function d() { return $this->s(); }
}
echo (new IpmH)->c(), ' ', (new IpmH)->d(), "\n";
?>
--EXPECT--
p
t
pr
base child
--CLEAN--
<?php
