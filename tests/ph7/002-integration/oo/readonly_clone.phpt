--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone preserves readonly/typed init state; a type-rejected readonly init can retry
--FILE--
<?php
class ReadonlyClone {
    public function __construct(public readonly int $x) {}
}
$a = new ReadonlyClone(7);
$b = clone $a;
echo $b->x, "\n";              // clone keeps the value and stays readable

// a typed property survives clone without losing its initialized state
class TypedClone {
    public int $n;
    public function __construct() { $this->n = 3; }
}
$t = new TypedClone;
$tc = clone $t;
echo $tc->n, "\n";

// a type-rejected readonly initialization does not burn the one allowed write
class ReadonlyRetry {
    public readonly int $y;
    public function __construct() {
        try { $this->y = "bad"; } catch (\TypeError $e) { echo "caught\n"; }
        $this->y = 9;
    }
}
$r = new ReadonlyRetry;
echo $r->y, "\n";
?>
--EXPECT--
7
3
caught
9
--CLEAN--
<?php
