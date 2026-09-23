--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A method Closure reports the class that DECLARED its callee as its scope
--FILE--
<?php
trait CsvcTrait { private function tp() { return "tp"; } }
class CsvcBase {
    use CsvcTrait;
    private function priv() { return "priv"; }
    public static function stat() { return "stat"; }
    public function mkPriv() { return $this->priv(...); }
    public function mkTrait() { return $this->tp(...); }
}
class CsvcKid extends CsvcBase {}

function csvcScope($c) {
    $s = (new ReflectionFunction($c))->getClosureScopeClass();
    return $s ? $s->getName() : 'NULL';
}
function csvcCalled($c) {
    $s = (new ReflectionFunction($c))->getClosureCalledClass();
    return $s ? $s->getName() : 'NULL';
}

$k = new CsvcKid();
/* The scope is where the method was DECLARED; the called class is what the call went
 * THROUGH -- two different questions that a single recorded class cannot answer. */
echo "inherited-scope:", csvcScope($k->mkPriv()), "\n";
echo "inherited-called:", csvcCalled($k->mkPriv()), "\n";
/* A trait is composed INTO the using class, so php names that class, never the trait. */
echo "trait-scope:", csvcScope($k->mkTrait()), "\n";
/* A static callable carries no receiver: its called class is the one it named. */
echo "static-scope:", csvcScope(CsvcKid::stat(...)), "\n";
echo "static-called:", csvcCalled(CsvcKid::stat(...)), "\n";
echo "fromCallable-scope:", csvcScope(Closure::fromCallable([$k, 'mkPriv'])), "\n";
echo "fromCallable-called:", csvcCalled(Closure::fromCallable([$k, 'mkPriv'])), "\n";
echo "reflection-scope:", csvcScope((new ReflectionMethod('CsvcKid', 'priv'))->getClosure($k)), "\n";
/* A plain closure declared at global scope has neither until it is bound. */
$plain = function () {};
echo "plain-scope:", csvcScope($plain), "\n";
echo "bound-scope:", csvcScope($plain->bindTo($k, CsvcBase::class)), "\n";
echo "bound-called:", csvcCalled($plain->bindTo($k, CsvcBase::class)), "\n";
?>
--EXPECT--
inherited-scope:CsvcBase
inherited-called:CsvcKid
trait-scope:CsvcBase
static-scope:CsvcBase
static-called:CsvcKid
fromCallable-scope:CsvcBase
fromCallable-called:CsvcKid
reflection-scope:CsvcBase
plain-scope:NULL
bound-scope:CsvcBase
bound-called:CsvcKid
--CLEAN--
<?php
