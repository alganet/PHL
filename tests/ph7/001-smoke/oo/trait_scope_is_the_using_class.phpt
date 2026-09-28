--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait method's SCOPE is the class that composed it, so the class's protected members are its own
--DESCRIPTION--
php composes a trait method INTO the using class at compile time, so the scope its
code executes in IS that class -- zend_get_executed_scope() never answers a trait.
PHL shares a trait method by pointer and its declaring class stays the trait, so
the executing scope answered the trait and every non-public access from a trait
body was refused: a protected property of the trait's own, one declared by the
using class, a protected METHOD either way, and `isset()` on one, which answered
a silent `false` rather than raising. Private access happened to work through a
pair of "the caller is a trait used by this class" grants bolted onto the private
branch; protected had no such thing. The scope is now the composing class from
the start and those grants have nothing left to do.

The composing class is found by walking the RECEIVER's ancestry (the class the
call was made through, for a static one) -- and through NESTED trait use, since
php flattens `class C { use Outer; } trait Outer { use Inner; }` into C whole. The
one existing walk that did not recurse reported the inner trait as a member's
owner, which is how an alias made in an outer trait lost its visibility.
--FILE--
<?php
trait TscT {
    protected array $p = ['a'];
    protected static $ps = 's';
    private $priv = 'v';
    protected function prot() { return 'prot'; }
    private static function privS() { return 'privS'; }

    public function readOwn()      { return $this->p; }
    public function writeOwn()     { $this->p = ['b']; return $this->p; }
    public function issetOwn()     { return isset($this->p); }
    public function callProt()     { return $this->prot(); }
    public function readClassProt(){ return $this->q; }
    public function callClassProt(){ return $this->classProt(); }
    public function readPriv()     { return $this->priv; }
    public function peer(self $o)  { return $o->p; }
    public static function readStatic()  { return static::$ps; }
    public static function callPrivS()   { return self::privS(); }
}
class TscC {
    use TscT;
    protected $q = 'q';
    protected function classProt() { return 'classProt'; }
}
class TscKid extends TscC {}

$c = new TscC;
var_dump($c->readOwn(), $c->writeOwn(), $c->issetOwn(), $c->callProt());
var_dump($c->readClassProt(), $c->callClassProt(), $c->readPriv(), $c->peer(new TscC));
var_dump(TscC::readStatic(), TscC::callPrivS());
/* A SUBCLASS of the composing class runs the same code with the same scope. */
$k = new TscKid;
var_dump($k->readOwn(), $k->callProt(), $k->readClassProt());

/* Nested composition: php flattens `class { use Outer; } trait Outer { use Inner; }`
 * into the class whole, so an Inner member is the CLASS's from anywhere in there. */
trait TscInner {
    protected $ip = 'ip';
    public function hi() { return 'hi'; }
    protected function ih() { return 'ih'; }
}
trait TscOuter {
    use TscInner { hi as protected pHi; }
    public function reach() { return [$this->ip, $this->ih(), $this->pHi()]; }
}
class TscNested { use TscOuter; }
var_dump((new TscNested)->reach());

/* And the refusals php still makes: two unrelated classes composing one trait are
 * not in a hierarchy, so neither reaches the other's protected member. */
trait TscPeek { public function peek($o) { return $o->p; } }
class TscA { use TscPeek; protected $p = 1; }
class TscB { use TscPeek; protected $p = 2; }
try { (new TscA)->peek(new TscB); } catch (Throwable $e) {
    var_dump(get_class($e), $e->getMessage());
}

/* A GENERATOR declared in a trait runs in a detached frame created before the
 * receiver is known: it carries the called-scope too, or its body could not
 * reach the class's protected members -- and the class that DRIVES it must not
 * change the answer. */
trait TscGen {
    protected $g = 'g';
    protected function gh() { return 'gh'; }
    public function gen() { yield $this->g; yield $this->gh(); }
}
class TscGenC { use TscGen; }
class TscDriver {
    public function drive() { $o = []; foreach ((new TscGenC)->gen() as $v) { $o[] = $v; } return $o; }
}
var_dump((new TscDriver)->drive());

/* A closure written in a trait body inherits the composed scope the same way. */
trait TscClo {
    private $cp = 'cp';
    private static $cs = 'cs';
    public function viaClosure() { $f = function () { return $this->cp; }; return $f(); }
    public static function viaStatic() { $f = static function () { return self::$cs; }; return $f(); }
}
class TscCloC { use TscClo; }
var_dump((new TscCloC)->viaClosure(), TscCloC::viaStatic());
?>
--EXPECT--
array(1) {
  [0]=>
  string(1) "a"
}
array(1) {
  [0]=>
  string(1) "b"
}
bool(true)
string(4) "prot"
string(1) "q"
string(9) "classProt"
string(1) "v"
array(1) {
  [0]=>
  string(1) "a"
}
string(1) "s"
string(5) "privS"
array(1) {
  [0]=>
  string(1) "a"
}
string(4) "prot"
string(1) "q"
array(3) {
  [0]=>
  string(2) "ip"
  [1]=>
  string(2) "ih"
  [2]=>
  string(2) "hi"
}
string(5) "Error"
string(41) "Cannot access protected property TscB::$p"
array(2) {
  [0]=>
  string(1) "g"
  [1]=>
  string(2) "gh"
}
string(2) "cp"
string(2) "cs"
--CLEAN--
<?php
