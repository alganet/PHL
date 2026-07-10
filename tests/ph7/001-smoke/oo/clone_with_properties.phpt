--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.5 clone($obj, [updates]) with property updates
--FILE--
<?php
class Point {
    public function __construct(public int $x = 0, public int $y = 0) {}
    public function __clone() { echo "clone sees x={$this->x}\n"; }
}
$p = new Point(1, 2);
// property updates apply AFTER __clone(); the original is untouched
$q = clone($p, ['x' => 10]);
echo "$q->x,$q->y / $p->x,$p->y\n";       // 10,2 / 1,2
// multiple updates
$r = clone($p, ['x' => 5, 'y' => 6]);
echo "$r->x,$r->y\n";                     // 5,6
// no-updates call form equals the statement form
$s = clone($p);
echo "$s->x,$s->y\n";                     // 1,2
$t = clone $p;
echo "$t->x,$t->y\n";                     // 1,2
// named arguments
$u = clone(object: $p, withProperties: ['y' => 99]);
echo "$u->x,$u->y\n";                     // 1,99
// typed property: a numeric string coerces like a normal store
$v = clone($p, ['x' => "42"]);
echo "$v->x\n";                           // 42
// a parenthesized clone() call is a valid arrow/method left operand
echo (clone($p, ['x' => 8]))->x, "\n";    // 8
echo (clone($p))->x, "\n";                // 1

// readonly: re-initialized in-scope, rejected from outside
final class Temp {
    public function __construct(public readonly float $c) {}
    public function withC(float $c): static { return clone($this, ['c' => $c]); }
}
$a = new Temp(20.0);
$b = $a->withC(100.0);
echo "$a->c $b->c\n";                     // 20 100
try {
    clone($a, ['c' => 0.0]);              // from global scope: not allowed
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}

// visibility: private property update from outside is an Error
class Secret { private $s = 'a'; }
try {
    clone(new Secret(), ['s' => 'b']);
} catch (\Error $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
clone sees x=1
10,2 / 1,2
clone sees x=1
5,6
clone sees x=1
1,2
clone sees x=1
1,2
clone sees x=1
1,99
clone sees x=1
42
clone sees x=1
8
clone sees x=1
1
20 100
Cannot modify protected(set) readonly property Temp::$c from global scope
Cannot access private property Secret::$s
--CLEAN--
<?php
