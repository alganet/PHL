--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jump() skips ahead without drawing: PCG by arithmetic, Xoshiro by polynomial
--DESCRIPTION--
Both 64-bit engines can move a long way down their stream without producing the
values in between, and they do it in completely different ways. PCG's whole
recurrence is one multiply-add, so N of them collapse into another multiply-add
found by squaring in log(N) rounds -- `jump(1000000)` costs what `jump(2)`
costs, and `jump(1)` has to land exactly where one generate() would have left
the state. Xoshiro has no such arithmetic, so its two jumps are the published
magic polynomials, worth 2^128 and 2^192 draws. A zero advance is a no-op and a
negative one is refused.
--FILE--
<?php
/* jump(1) is exactly one step of the recurrence. */
$a = new Random\Engine\PcgOneseq128XslRr64(0);
$a->generate();
$b = new Random\Engine\PcgOneseq128XslRr64(0);
$b->jump(1);
var_dump($a->__debugInfo() === $b->__debugInfo());

/* ...and jump(N) is N of them, however big N gets. */
$a = new Random\Engine\PcgOneseq128XslRr64(7);
foreach (range(1, 40) as $ignored) {
    $a->generate();
}
$b = new Random\Engine\PcgOneseq128XslRr64(7);
$b->jump(40);
var_dump($a->generate() === $b->generate());

$z = new Random\Engine\PcgOneseq128XslRr64(0);
$before = $z->__debugInfo();
$z->jump(0);
var_dump($before === $z->__debugInfo());
$z->jump(1000000);
print_r($z->__debugInfo()['__states']);

try { (new Random\Engine\PcgOneseq128XslRr64(0))->jump(-1); }
catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }

/* Xoshiro's two polynomials. */
$j = new Random\Engine\Xoshiro256StarStar(0);
$j->jump();
print_r($j->__debugInfo()['__states']);
$l = new Random\Engine\Xoshiro256StarStar(0);
$l->jumpLong();
print_r($l->__debugInfo()['__states']);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
Array
(
    [0] => de5638b40d8b61d7
    [1] => da095c71e1ce88a5
)
ValueError: Random\Engine\PcgOneseq128XslRr64::jump(): Argument #1 ($advance) must be greater than or equal to 0
Array
(
    [0] => 828da8d48cf5e4fe
    [1] => a3d5f77078cb57eb
    [2] => 0f72d22b192d6f07
    [3] => 7bd71071b71ca7b0
)
Array
(
    [0] => 678bf9c3ebdf65af
    [1] => 52d46d3a40b626bb
    [2] => bd66d118356768bf
    [3] => a0ff79829639994c
)
