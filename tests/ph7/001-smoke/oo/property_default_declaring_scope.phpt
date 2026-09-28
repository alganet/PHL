--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self:: and parent:: in a member initializer name the DECLARING class, wherever `new` ran
--DESCRIPTION--
A property default is evaluated at `new`, i.e. at an arbitrary point in some
other class's method — and the initializer's own mini-execution pushes no frame,
so that method's frame is still the current one. Without a marker saying "this
frame is running an initializer", `self::` resolved against the CALLER: `class M
{ const K = 7; public int $v = self::K; }` instantiated inside `F::make()` read
F::K, which is an Error when F has no K and, worse, F's OWN value when it has
one. `parent::` said `Class "parent" not found` and `self::class` answered F.
The same marker was missing where a class is MOUNTED (a class declared inside a
method: its static property defaults read the method's class) and where an enum
CASE's backing value is first materialized. A trait's members are composed into
the using class, so an initializer written in a trait resolves against that
class and not against the trait, which has no constants and no base.
--FILE--
<?php
class PddsBase { const BK = 'bk'; }
class PddsM extends PddsBase {
    const K = 7;
    public int $v = self::K;
    public array $a = [self::K => 'x'];
    public string $p = parent::BK;
    public string $c = self::class;
    public $n = self::K + 1;
}
class PddsSub extends PddsM { const K = 99; }

class PddsCaller {
    const K = 'WRONG';
    const BK = 'WRONG';
    public function inst()      { return (new PddsM)->v; }
    public function arr()       { return (new PddsM)->a; }
    public function par()       { return (new PddsM)->p; }
    public function cls()       { return (new PddsM)->c; }
    public function expr()      { return (new PddsM)->n; }
    public function sub()       { return (new PddsSub)->v; }
    public static function st() { return (new PddsM)->v; }
}
$f = new PddsCaller;
var_dump($f->inst(), $f->arr(), $f->par(), $f->cls(), $f->expr(), $f->sub());
var_dump(PddsCaller::st(), (new PddsM)->v);

/* A closure carries its creation site's class scope, and that is not the
 * initializer's scope either. */
class PddsClosure {
    const K = 'WRONG';
    public function go() { $f = function () { return (new PddsM)->v; }; return $f(); }
}
var_dump((new PddsClosure)->go());

/* An interface constant reached through self:: from the implementing class. */
interface PddsIface { const IK = 4; }
class PddsImpl implements PddsIface { public int $v = self::IK; }
var_dump((new PddsCaller)->inst() === 7, (new PddsImpl)->v);

/* Trait-declared members compose INTO the using class. */
trait PddsTrait {
    public string $tc = self::class;
    public string $tk = self::TK;
    public string $tp = parent::BK;
}
class PddsUser extends PddsBase { const TK = 'tk'; use PddsTrait; }
class PddsUserKid extends PddsUser {}
trait PddsOuter { use PddsTrait; }
class PddsOuterUser extends PddsBase { const TK = 'ok'; use PddsOuter; }
class PddsTraitCaller {
    const TK = 'WRONG';
    public function go(string $cls) { $o = new $cls; return [$o->tc, $o->tk, $o->tp]; }
}
$t = new PddsTraitCaller;
var_dump($t->go('PddsUser'), $t->go('PddsUserKid'), $t->go('PddsOuterUser'));
?>
--EXPECT--
int(7)
array(1) {
  [7]=>
  string(1) "x"
}
string(2) "bk"
string(5) "PddsM"
int(8)
int(7)
int(7)
int(7)
int(7)
bool(true)
int(4)
array(3) {
  [0]=>
  string(8) "PddsUser"
  [1]=>
  string(2) "tk"
  [2]=>
  string(2) "bk"
}
array(3) {
  [0]=>
  string(8) "PddsUser"
  [1]=>
  string(2) "tk"
  [2]=>
  string(2) "bk"
}
array(3) {
  [0]=>
  string(13) "PddsOuterUser"
  [1]=>
  string(2) "ok"
  [2]=>
  string(2) "bk"
}
--CLEAN--
<?php
