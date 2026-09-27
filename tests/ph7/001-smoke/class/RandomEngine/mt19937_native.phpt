--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Random\Engine\Mt19937 hands out the raw tempered word, both modes
--DESCRIPTION--
php 8.2's engine objects hand out BITS, not numbers: generate() returns the four
bytes of the raw tempered MT19937 word, little-endian, where mt_rand() would
have dropped the low bit first. A seed is truncated to 32 bits (PHP_INT_MAX and
-1 are the same engine), and $mode picks between MT19937 proper and php's
pre-7.1 broken twist -- a different sequence, which is the only reason to ask
for it. The state is the object's, so a clone keeps drawing where the original
was and neither one advances the other.
--FILE--
<?php
$e = new Random\Engine\Mt19937(1234);
foreach (range(1, 4) as $ignored) {
    printf("%08x ", unpack("V", $e->generate())[1]);
}
echo "\n";
var_dump($e instanceof Random\Engine);

/* A seed is 32 bits wide: these two engines are the same engine. */
$a = new Random\Engine\Mt19937(-1);
$b = new Random\Engine\Mt19937(PHP_INT_MAX);
var_dump($a->generate() === $b->generate());

/* php's pre-7.1 twist is a different sequence from the first word on. */
$std = new Random\Engine\Mt19937(1, MT_RAND_MT19937);
$php = new Random\Engine\Mt19937(1, @MT_RAND_PHP);
var_dump(bin2hex($std->generate()), bin2hex($php->generate()));

/* The state travels with the object. */
$src = new Random\Engine\Mt19937(9);
$src->generate();
$copy = clone $src;
var_dump($src->generate() === $copy->generate());
$src->generate();
var_dump($src->generate() === $copy->generate());

/* Nothing of the state is a property. */
var_dump(get_object_vars($src), (array)$src);

try {
    new Random\Engine\Mt19937(1, 7);
} catch (Throwable $t) {
    echo get_class($t), ': ', $t->getMessage(), "\n";
}
?>
--EXPECT--
31076b2f 7f66e2d3 9f428526 d15ddc35 
bool(true)
bool(true)
string(8) "25f4c16a"
string(8) "c91e5694"
bool(true)
bool(false)
array(0) {
}
array(0) {
}
ValueError: Random\Engine\Mt19937::__construct(): Argument #2 ($mode) must be either MT_RAND_MT19937 or MT_RAND_PHP
