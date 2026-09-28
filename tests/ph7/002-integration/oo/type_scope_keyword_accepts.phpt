--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The scope-keyword screens leave every declaration php accepts alone
--DESCRIPTION--
The accepting half of the three screens, which cannot share a file with a
refusal because a compile fatal runs nothing: a named class naming its own
`self` in an intersection, `: static` as a return type on a method and on a
closure, `parent` where there IS one, and a trait and a closure deferring both
questions to composition and binding.
--FILE--
<?php
class TskaC implements Countable, ArrayAccess {
    public function count(): int { return 0; }
    public function offsetExists(mixed $o): bool { return false; }
    public function offsetGet(mixed $o): mixed { return null; }
    public function offsetSet(mixed $o, mixed $v): void {}
    public function offsetUnset(mixed $o): void {}
}
class TskaRoot {}
trait TskaT {
    public function deferred(parent $a, self $b) {}
}
class TskaA extends TskaRoot {
    use TskaT;
    public function inter(self&Countable $a) {}
    public function ret(): static { return $this; }
    public function par(parent $a): parent { return $a; }
}
$closureRet = function (): static {};
$closureSelf = function (self $a) {};
$arrowParent = fn (parent $a) => $a;
$o = new TskaA;
var_dump($o->ret() === $o, $o->par(new TskaRoot) instanceof TskaRoot);
var_dump($closureRet instanceof Closure, $closureSelf instanceof Closure, $arrowParent instanceof Closure);
echo "accepted\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
accepted
--CLEAN--
<?php
