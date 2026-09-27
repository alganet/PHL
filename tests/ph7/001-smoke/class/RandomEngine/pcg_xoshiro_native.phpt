--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The two 64-bit engines draw php's own sequence, seed for seed
--DESCRIPTION--
PcgOneseq128XslRr64 and Xoshiro256StarStar are the reproducible engines php
added beside Mt19937, and both hand out eight bytes rather than four. Their
seed argument has three arms that mean different things: null asks the OS, an
int is MIXED into the state (PCG steps twice around it, Xoshiro runs it through
SplitMix64), and a STRING **is** the state -- which is why it must be exactly
as wide as the state and why a Xoshiro seeded with 32 NUL bytes is refused: all
zeroes is a fixed point of its update and the generator would answer 0 forever.
--FILE--
<?php
$p = new Random\Engine\PcgOneseq128XslRr64(0);
foreach (range(1, 4) as $ignored) {
    echo bin2hex($p->generate()), ' ';
}
echo "\n";
$x = new Random\Engine\Xoshiro256StarStar(0);
foreach (range(1, 4) as $ignored) {
    echo bin2hex($x->generate()), ' ';
}
echo "\n";
var_dump(strlen($p->generate()), $p instanceof Random\Engine, $x instanceof Random\Engine);

/* A 128-bit string seed is the state, high word first. */
$a = new Random\Engine\PcgOneseq128XslRr64(str_repeat("\x00", 16));
$b = new Random\Engine\PcgOneseq128XslRr64(0);
var_dump($a->generate() === $b->generate());

/* A 256-bit string seed lands in the state untouched. */
$s = new Random\Engine\Xoshiro256StarStar("\x01" . str_repeat("\x00", 31));
print_r($s->__debugInfo()['__states']);

foreach ([['Random\Engine\PcgOneseq128XslRr64', 'short'],
          ['Random\Engine\Xoshiro256StarStar', 'short'],
          ['Random\Engine\Xoshiro256StarStar', "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"]] as [$class, $seed]) {
    try {
        new $class($seed);
        echo "accepted\n";
    } catch (Throwable $t) {
        echo get_class($t), ': ', $t->getMessage(), "\n";
    }
}
?>
--EXPECT--
f1f895e696010701 93449fc540c83e70 fa443a4b915449e5 5e28b904f20f1396 
b4f275cb365fec99 2a455649781f6ebf e0e633499d845f1a 2c2d2d26f194a56a 
int(8)
bool(true)
bool(true)
bool(true)
Array
(
    [0] => 0100000000000000
    [1] => 0000000000000000
    [2] => 0000000000000000
    [3] => 0000000000000000
)
ValueError: Random\Engine\PcgOneseq128XslRr64::__construct(): Argument #1 ($seed) must be a 16 byte (128 bit) string
ValueError: Random\Engine\Xoshiro256StarStar::__construct(): Argument #1 ($seed) must be a 32 byte (256 bit) string
ValueError: Random\Engine\Xoshiro256StarStar::__construct(): Argument #1 ($seed) must not consist entirely of NUL bytes
