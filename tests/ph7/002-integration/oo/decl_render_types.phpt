--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An incompatible declaration names the class that DECLARED it, with self resolved
--DESCRIPTION--
Both sides of the sentence are the DECLARING class, not the one the walk reached
the method through: the parent here is inherited by DrtMid and still reads DrtTop,
and the child comes from a trait and reads the class that composed it. `self` and
`parent` resolve against that same class, `static` stays as written, and `iterable`
is spelled as the two types it stands for.
--FILE--
<?php
class DrtRoot {}
class DrtTop extends DrtRoot {
    public function f(self $a, parent $b, iterable $c, ?iterable $d, int $e): static { return $this; }
}
class DrtMid extends DrtTop {}
trait DrtT {
    public function f(self $a, parent $b, iterable $c, ?iterable $d, string $e): static { return $this; }
}
class DrtLeaf extends DrtMid { use DrtT; }
echo "unreached\n";
?>
--EXPECTF--
%ADeclaration of DrtLeaf::f(DrtLeaf $a, DrtMid $b, Traversable|array $c, Traversable|array|null $d, string $e): static must be compatible with DrtTop::f(DrtTop $a, DrtRoot $b, Traversable|array $c, Traversable|array|null $d, int $e): static%A
--CLEAN--
<?php
