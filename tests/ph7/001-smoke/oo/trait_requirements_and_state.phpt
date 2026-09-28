--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait's abstract is a requirement an INHERITED method satisfies, and each using class composes its own state
--DESCRIPTION--
Three things a trait is not: it is not a member list, it is not a fixed modifier
order, and it is not shared state. A trait's `abstract` is a REQUIREMENT, and php
lets a method inherited from the BASE answer it -- PHL applied the trait before
inheriting, so the requirement shadowed the very method that satisfies it and the
class was "abstract and must therefore be declared abstract". The satisfying
declaration is still checked for compatibility, and php words that one the other
way round: the class that PROVIDES the method, against the trait that asked.
Member modifiers are a SET in php's parser, so every order of
abstract/static/visibility is one declaration; both member loops walked a fixed
ladder and refused the rest. And php composes a trait into each using class
separately, so a trait's static property is one slot per class and a trait
constant is evaluated per class -- copying the record by pointer gave every using
class the same slot and the same memoized value. A SUBCLASS composes nothing and
still shares its parent's, as php does.
--FILE--
<?php
/* A trait's `abstract` is a REQUIREMENT, and php lets an INHERITED method
 * satisfy it -- the trait is composed before the base is inherited, so the
 * requirement used to shadow the very method that answers it. */
trait TabT {
    abstract public function need(): string;
    abstract public static function needS(): string;
    abstract protected function needP(): string;
    public function use2() { return [$this->need(), static::needS(), $this->needP()]; }
}
class TabP {
    public function need(): string { return 'parent'; }
    public static function needS(): string { return 'parentS'; }
    public function needP(): string { return 'parentP'; }   /* php: a PUBLIC method satisfies a protected requirement */
}
class TabC extends TabP { use TabT; }
var_dump((new TabC)->use2());

/* Modifiers are a SET: every order of abstract / static / visibility is one
 * declaration, in a class body and in a trait body alike. */
abstract class TabM {
    abstract static public function a();
    static abstract public function b();
    public static abstract function c();
    abstract public static function d();
}
class TabMi extends TabM {
    public static function a() { return 'a'; }
    public static function b() { return 'b'; }
    public static function c() { return 'c'; }
    public static function d() { return 'd'; }
}
var_dump(TabMi::a(), TabMi::b(), TabMi::c(), TabMi::d());
trait TabOrders {
    abstract static public function e();
    static abstract public function f();
}
class TabOc { use TabOrders; public static function e(){ return 'e'; } public static function f(){ return 'f'; } }
var_dump(TabOc::e(), TabOc::f());

/* php composes a trait into each using class SEPARATELY: a static property is
 * one slot per class, and a constant is evaluated per class. A SUBCLASS still
 * shares the composing class's slot -- it did not compose anything. */
trait TabState {
    public static $c = 0;
    const K = self::J;
    public static function inc() { return ++static::$c; }
}
class TabA { use TabState; const J = 'a'; }
class TabB { use TabState; const J = 'b'; }
class TabAKid extends TabA {}
TabA::inc(); TabA::inc(); TabB::inc();
var_dump(TabA::$c, TabB::$c, TabA::K, TabB::K);
TabAKid::$c = 9;
var_dump(TabA::$c, TabAKid::$c);

/* ...and through a nested composition, the class that used the OUTER trait is
 * the one that gets the slot. */
trait TabInner { public static $n = 0; }
trait TabOuter { use TabInner; }
class TabX { use TabOuter; }
class TabY { use TabOuter; }
TabX::$n = 4;
var_dump(TabX::$n, TabY::$n);
?>
--EXPECT--
array(3) {
  [0]=>
  string(6) "parent"
  [1]=>
  string(7) "parentS"
  [2]=>
  string(7) "parentP"
}
string(1) "a"
string(1) "b"
string(1) "c"
string(1) "d"
string(1) "e"
string(1) "f"
int(2)
int(1)
string(1) "a"
string(1) "b"
int(9)
int(9)
int(4)
int(0)
--CLEAN--
<?php
