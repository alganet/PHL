--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: a trait method's visibility is the COMPOSING class's, `as` modifiers included
--DESCRIPTION--
php flattens a trait into the class that uses it: the composed method's scope IS
that class, so an `as private`/`as protected` adaptation is enforced at the call,
a protected trait method is reachable from a SUBCLASS of the composing class, and
every refusal names the composing class — never the trait, which no longer exists
at run time.
--FILE--
<?php
trait TcvT {
    public function pub() { return 'pub'; }
    protected function prot() { return 'prot'; }
    private function priv() { return 'priv'; }
    public static function sPub() { return 'sPub'; }
}
class TcvBase {
    use TcvT {
        pub as protected pubProt;
        pub as private pubPriv;
        sPub as private sHidden;
        prot as public opened;
    }
    public function inside() {
        return $this->priv() . ',' . $this->prot() . ',' . $this->pubProt() . ',' . $this->pubPriv();
    }
}
class TcvKid extends TcvBase {
    public function fromKid() { return $this->prot() . ',' . $this->pubProt(); }
}
class TcvMagic {
    use TcvT { pub as private pubPriv; }
    public function __call($n, $a) { return "__call:$n"; }
}
function tcvSay(callable $f) {
    try { echo $f(), "\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
$b = new TcvBase;
$k = new TcvKid;

/* Inside the composing class every spelling reaches. */
echo $b->pub(), ' ', $b->inside(), "\n";
/* A SUBCLASS reaches the protected ones — it uses no trait of its own. */
echo $k->fromKid(), "\n";
/* ...and the adaptation that OPENS a protected method is public everywhere. */
echo $b->opened(), ' ', $k->opened(), "\n";

/* From outside, the composing class's rules apply and its name is reported. */
tcvSay(fn() => $b->prot());
tcvSay(fn() => $b->priv());
tcvSay(fn() => $b->pubProt());
tcvSay(fn() => $b->pubPriv());
tcvSay(fn() => $k->pubPriv());
tcvSay(fn() => TcvBase::sHidden());
/* The original public name is untouched by an alias made from it. */
echo $b->pub(), ' ', TcvBase::sPub(), "\n";

/* The callable spellings answer the same rule, and their reason names the class
   the callable SPELLED. */
var_dump(is_callable([$b, 'pubProt']), is_callable([$k, 'prot']), is_callable([$b, 'opened']));
tcvSay(fn() => call_user_func([$k, 'pubPriv']));
/* An inaccessible name routes to the catch-all when the class has one. */
echo (new TcvMagic)->pubPriv(), "\n";
?>
--EXPECT--
pub priv,prot,pub,pub
prot,pub
prot prot
Error: Call to protected method TcvBase::prot() from global scope
Error: Call to private method TcvBase::priv() from global scope
Error: Call to protected method TcvBase::pubProt() from global scope
Error: Call to private method TcvBase::pubPriv() from global scope
Error: Call to private method TcvBase::pubPriv() from global scope
Error: Call to private method TcvBase::sHidden() from global scope
pub sPub
bool(false)
bool(false)
bool(true)
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method TcvKid::pubPriv()
__call:pubPriv
--CLEAN--
<?php
