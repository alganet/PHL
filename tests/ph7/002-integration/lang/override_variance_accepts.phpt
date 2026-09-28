--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The override lattice ACCEPTS every subtype php accepts
--DESCRIPTION--
The other half of the variance rules: a comparator strict enough to refuse what
php refuses must still let through every declaration php compiles. `never` under
anything, `static` under a class the declaring one is an instance of, `false`
under `bool`, `array` and a Traversable class under `iterable`, a widened union
and an intersection over a class that implements both, and a variadic tail
answering for the parameters past its own position.
--FILE--
<?php
class OvkRoot {}
class OvkBase extends OvkRoot {
    public function ret(): OvkBase { return $this; }
    public function nev(): int { return 1; }
    public function stat(): OvkRoot { return $this; }
    public function bool(): bool { return true; }
    public function iter(): iterable { return []; }
    public function uni(int $a): int { return $a; }
    public function inter(OvkC $a): void {}
    public function many($a, $b, $c) {}
    public function opt(int $a) {}
}
class OvkC implements Countable, ArrayAccess {
    public function count(): int { return 0; }
    public function offsetExists(mixed $o): bool { return false; }
    public function offsetGet(mixed $o): mixed { return null; }
    public function offsetSet(mixed $o, mixed $v): void {}
    public function offsetUnset(mixed $o): void {}
}
class OvkSub extends OvkBase {
    public function ret(): static { return $this; }
    public function nev(): never { throw new Exception(); }
    public function stat(): static { return $this; }
    public function bool(): false { return false; }
    public function iter(): array { return []; }
    public function uni(int|string $a): int { return 1; }
    public function inter(Countable&ArrayAccess $a): void {}
    public function many(...$v) {}
    public function opt(int $a, string $b = 'x', int ...$rest) {}
}
$o = new OvkSub;
var_dump($o->stat() instanceof OvkSub, $o->bool(), $o->iter(), $o->uni('7'));
echo "accepted\n";
?>
--EXPECT--
bool(true)
bool(false)
array(0) {
}
int(1)
accepted
--CLEAN--
<?php
