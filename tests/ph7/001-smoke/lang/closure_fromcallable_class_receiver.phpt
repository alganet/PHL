--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure::fromCallable over a class-named method binds the calling frame's $this and called class at creation
--FILE--
<?php
class FcRcvP {
  public $tag = 'p';
  function m() { return isset($this) ? get_class($this) . '/' . static::class . '/' . $this->tag : 'none'; }
  static function s() { return static::class; }
  private function priv() { return 'priv'; }
  function __call($n, $a) { return "__call $n on " . get_class($this); }
  static function __callStatic($n, $a) { return "__callStatic $n/" . static::class; }
}
class FcRcvA extends FcRcvP {
  public $tag = 'a';
  function make() {
    $o = [];
    foreach (['arr' => ['FcRcvP', 'm'], 'str' => 'FcRcvP::m', 'own' => ['FcRcvA', 'm'],
              'static' => ['FcRcvP', 's'], 'static-str' => 'FcRcvP::s',
              'magic' => ['FcRcvA', 'zz'], 'magic-str' => 'FcRcvA::zz',
              'priv' => ['FcRcvP', 'priv'], 'priv-str' => 'FcRcvP::priv'] as $k => $cb) {
      $o[$k] = Closure::fromCallable($cb);
    }
    return $o;
  }
  static function smake() {
    return ['static-scope' => Closure::fromCallable(['FcRcvA', 's']),
            'magic-static' => Closure::fromCallable(['FcRcvA', 'zz']),
            'magic-static-str' => Closure::fromCallable('FcRcvP::zz')];
  }
}
class FcRcvC extends FcRcvA { public $tag = 'c'; }
class FcRcvD {
  function make() {
    return ['unrelated' => Closure::fromCallable(['FcRcvP', 's']),
            'unrelated-magic' => Closure::fromCallable(['FcRcvP', 'zz'])];
  }
}
class FcRcvQ {
  static function __callStatic($n, $a) { return "Q::__callStatic $n/" . static::class; }
  function i() { return isset($this) ? 'bound ' . get_class($this) : 'unbound'; }
}
class FcRcvR extends FcRcvQ {
  function make() {
    return ['str' => Closure::fromCallable('FcRcvQ::zz'),
            'i-arr' => Closure::fromCallable(['FcRcvQ', 'i']),
            'i-str' => Closure::fromCallable('FcRcvQ::i')];
  }
}
/* Every closure is called OUTSIDE the method that made it. */
foreach ([(new FcRcvC)->make(), (new FcRcvA)->make(), FcRcvA::smake(), FcRcvC::smake(),
          (new FcRcvD)->make(), (new FcRcvR)->make()] as $set) {
  foreach ($set as $k => $f) {
    $r = new ReflectionFunction($f);
    $t = $r->getClosureThis();
    echo str_pad($k, 16), ' this=', $t ? get_class($t) : 'null',
      ' scope=', $r->getClosureScopeClass()?->name ?? 'null',
      ' called=', $r->getClosureCalledClass()?->name ?? 'null', ' -> ', $f(), "\n";
  }
}
/* A dynamic first-class callable over the same value binds nothing. */
class FcRcvE { static function s() { return static::class; } function make() { $v = ['FcRcvE', 's']; return $v(...); } }
class FcRcvF extends FcRcvE {}
$f = (new FcRcvF)->make();
echo (new ReflectionFunction($f))->getClosureCalledClass()->name, ' ', $f(), "\n";
?>
--EXPECT--
arr              this=FcRcvC scope=FcRcvP called=FcRcvC -> FcRcvC/FcRcvC/c
str              this=FcRcvC scope=FcRcvP called=FcRcvC -> FcRcvC/FcRcvC/c
own              this=FcRcvC scope=FcRcvP called=FcRcvC -> FcRcvC/FcRcvC/c
static           this=null scope=FcRcvP called=FcRcvC -> FcRcvC
static-str       this=null scope=FcRcvP called=FcRcvC -> FcRcvC
magic            this=FcRcvC scope=FcRcvP called=FcRcvC -> __call zz on FcRcvC
magic-str        this=null scope=FcRcvP called=FcRcvC -> __callStatic zz/FcRcvC
priv             this=FcRcvC scope=FcRcvP called=FcRcvC -> __call priv on FcRcvC
priv-str         this=null scope=FcRcvP called=FcRcvC -> __callStatic priv/FcRcvC
arr              this=FcRcvA scope=FcRcvP called=FcRcvA -> FcRcvA/FcRcvA/a
str              this=FcRcvA scope=FcRcvP called=FcRcvA -> FcRcvA/FcRcvA/a
own              this=FcRcvA scope=FcRcvP called=FcRcvA -> FcRcvA/FcRcvA/a
static           this=null scope=FcRcvP called=FcRcvA -> FcRcvA
static-str       this=null scope=FcRcvP called=FcRcvA -> FcRcvA
magic            this=FcRcvA scope=FcRcvP called=FcRcvA -> __call zz on FcRcvA
magic-str        this=null scope=FcRcvP called=FcRcvA -> __callStatic zz/FcRcvA
priv             this=FcRcvA scope=FcRcvP called=FcRcvA -> __call priv on FcRcvA
priv-str         this=null scope=FcRcvP called=FcRcvA -> __callStatic priv/FcRcvA
static-scope     this=null scope=FcRcvP called=FcRcvA -> FcRcvA
magic-static     this=null scope=FcRcvP called=FcRcvA -> __callStatic zz/FcRcvA
magic-static-str this=null scope=FcRcvP called=FcRcvP -> __callStatic zz/FcRcvP
static-scope     this=null scope=FcRcvP called=FcRcvA -> FcRcvA
magic-static     this=null scope=FcRcvP called=FcRcvA -> __callStatic zz/FcRcvA
magic-static-str this=null scope=FcRcvP called=FcRcvP -> __callStatic zz/FcRcvP
unrelated        this=null scope=FcRcvP called=FcRcvP -> FcRcvP
unrelated-magic  this=null scope=FcRcvP called=FcRcvP -> __callStatic zz/FcRcvP
str              this=null scope=FcRcvQ called=FcRcvR -> Q::__callStatic zz/FcRcvR
i-arr            this=FcRcvR scope=FcRcvQ called=FcRcvR -> bound FcRcvR
i-str            this=FcRcvR scope=FcRcvQ called=FcRcvR -> bound FcRcvR
FcRcvE FcRcvE
