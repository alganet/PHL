--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface's own declaration wins over every parent's, not just the first
--DESCRIPTION--
`interface B extends A, S` has two parents and php lets B restate a member of
either. Only the FIRST was inherited after the body here; every one after it had
its constants and method signatures copied in BEFORE the body was read, so B's
own `public function g();` collided with the very name it was restating
("Cannot redeclare B::g()") -- a declaration php-di writes and php accepts. The
extra parents are collected now and applied where the first one is.

The check php makes at that point came with it: a restated method must be
COMPATIBLE with the parent's, which this engine asked for no interface at all.
And an unimplemented method is attributed to the interface that first ASKED for
it -- a walk that has to follow both containers an interface reaches its parents
through.
--FILE--
<?php
/* B restates a member of each parent, and of a native one. */
interface ImpA { const KA = 'a'; public function fa(): string; }
interface ImpS { const KS = 's'; public function fs(): string; }
interface ImpB extends ImpA, ImpS {
    const KA = 'a';
    const KS = 's';
    public function fa(): string;
    public function fs(): string;
}
class ImpC implements ImpB {
    public function fa(): string { return 'fa'; }
    public function fs(): string { return 'fs'; }
}
$impC = new ImpC;
var_dump($impC->fa(), $impC->fs(), ImpB::KA, ImpB::KS);
var_dump($impC instanceof ImpA, $impC instanceof ImpS, $impC instanceof ImpB);

/* Three parents, and one of them the engine's own. */
interface ImpX { public function x(); }
interface ImpY { public function y(); }
interface ImpZ extends ImpX, ImpY, \Countable {
    public function x();
    public function y();
    public function count(): int;
}
class ImpW implements ImpZ {
    public function x() { return 'x'; }
    public function y() { return 'y'; }
    public function count(): int { return 7; }
}
$impW = new ImpW;
var_dump($impW->x(), $impW->y(), count($impW), $impW instanceof Countable);

/* A parent's constant may also be given a DIFFERENT value by a child interface. */
interface ImpK1 { const K = 1; }
interface ImpK2 extends ImpK1 { const K = 2; }
var_dump(ImpK1::K, ImpK2::K);

/* An unimplemented method is attributed to the interface that first asked. */
interface ImpN1 { public function n1(): int; }
interface ImpN2 { public function n2(): int; }
interface ImpN extends ImpN1, ImpN2 {}
abstract class ImpAbs implements ImpN { public function n1(): int { return 1; } }
$impR = new ReflectionClass('ImpAbs');
$impNames = array_map(fn($m) => $m->getName() . ($m->isAbstract() ? ':abstract' : ':concrete'),
    $impR->getMethods());
sort($impNames);
var_dump($impR->isAbstract(), $impNames);
?>
--EXPECT--
string(2) "fa"
string(2) "fs"
string(1) "a"
string(1) "s"
bool(true)
bool(true)
bool(true)
string(1) "x"
string(1) "y"
int(7)
bool(true)
int(1)
int(2)
bool(true)
array(2) {
  [0]=>
  string(11) "n1:concrete"
  [1]=>
  string(11) "n2:abstract"
}
--CLEAN--
<?php
