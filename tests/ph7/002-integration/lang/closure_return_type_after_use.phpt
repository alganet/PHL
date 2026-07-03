--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure return types in the post-use position: scalar, nullable, intersection; arrow-fn intersection
--FILE--
<?php
class Both implements Countable, ArrayAccess {
    public function count(): int { return 7; }
    public function offsetExists($o): bool { return false; }
    public function offsetGet($o): mixed { return null; }
    public function offsetSet($o, $v): void {}
    public function offsetUnset($o): void {}
}
$a = 10;
$f1 = function (int $n) use ($a): int {
    return $n + $a;
};
echo $f1(1), "\n";
$f2 = function (int $n) use ($a): ?string {
    return $n > 0 ? "pos" : null;
};
echo $f2(5), "\n";
echo var_export($f2(-1), true), "\n";
$f3 = function () use ($a): Countable&ArrayAccess {
    return new Both();
};
echo count($f3()), "\n";
$f4 = fn(): Countable&ArrayAccess => new Both();
echo count($f4()), "\n";
?>
--EXPECT--
11
pos
NULL
7
7
--CLEAN--
<?php
