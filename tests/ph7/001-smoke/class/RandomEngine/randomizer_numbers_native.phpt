--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Random\Randomizer turns an engine's bits into ints, bytes and floats
--DESCRIPTION--
The Randomizer is the consumer half of php 8.2's split: it holds an engine it
may never swap (readonly) and shapes that engine's bits. How MANY draws each
answer costs is visible from outside and is part of the contract. nextInt() is
exactly ONE draw shifted down a bit, so a 32-bit engine answers 31 bits there.
getInt() picks its width from the SPAN rather than from the engine, so the same
engine spends one draw on a 32-bit range and two on a wider one -- and a span
that is not a power of two has to REJECT the tail that would bias the modulo.
getBytes() takes each draw whole and cuts the last one short, discarding the
surplus rather than carrying it into the next call.
--FILE--
<?php
$mt = fn() => new Random\Randomizer(new Random\Engine\Mt19937(1234));
$pcg = fn() => new Random\Randomizer(new Random\Engine\PcgOneseq128XslRr64(1234));

/* nextInt() is one draw >> 1. */
$e = new Random\Engine\Mt19937(1234);
$raw = unpack('V', $e->generate())[1];
var_dump($mt()->nextInt() === $raw >> 1);

$r = $mt();
foreach (range(1, 4) as $ignored) {
    echo $r->nextInt(), ' ';
}
echo "\n";
$r = $pcg();
foreach (range(1, 4) as $ignored) {
    echo $r->nextInt(), ' ';
}
echo "\n";

/* getInt(): a 32-bit span costs one word, a wider one costs two. */
$r = $mt();
foreach (range(1, 6) as $ignored) {
    echo $r->getInt(0, 0xFFFFFFFF), ' ';
}
echo "\n";
$r = $mt();
foreach (range(1, 6) as $ignored) {
    echo $r->getInt(0, PHP_INT_MAX), ' ';
}
echo "\n";
/* ...and a span that is not a power of two rejects, so the draws do not line up
   with either of the above. */
$r = $mt();
foreach (range(1, 6) as $ignored) {
    echo $r->getInt(0, 10), ' ';
}
echo "\n";
var_dump($mt()->getInt(7, 7), $mt()->getInt(PHP_INT_MIN, PHP_INT_MAX));

/* getBytes(): whole draws, last one cut short, surplus dropped. */
$r = $mt();
echo bin2hex($r->getBytes(10)), ' ', bin2hex($r->getBytes(3)), "\n";
$r = $pcg();
echo bin2hex($r->getBytes(10)), ' ', bin2hex($r->getBytes(3)), "\n";

/* nextFloat() is 53 bits off the TOP of a 64-bit draw. */
$r = $mt();
foreach (range(1, 3) as $ignored) {
    printf('%.17g ', $r->nextFloat());
}
echo "\n";

/* The engine is the Randomizer's, and it is readonly. */
$eng = new Random\Engine\Mt19937(1);
$rz = new Random\Randomizer($eng);
var_dump($rz->engine === $eng);
try { $rz->engine = new Random\Engine\Mt19937(2); }
catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
var_dump((new Random\Randomizer())->engine instanceof Random\Engine\Secure);
/* Drawing through the Randomizer advances the engine the caller still holds. */
$before = bin2hex($eng->generate());
$rz->nextInt();
var_dump($before !== bin2hex($eng->generate()));

foreach ([fn() => $mt()->getInt(5, 1), fn() => $mt()->getBytes(0), fn() => $mt()->getBytes(-1)] as $bad) {
    try { $bad(); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
}
?>
--EXPECT--
bool(true)
411284887 1068724585 1335968403 1756294682 
4632401390016331254 2271882022817707431 1390279217326812967 8791373242050580190 
822569775 2137449171 2671936806 3512589365 1880026316 2629000564 
9180274287129881391 5863084412769568038 2068099408570805452 5005706978665513624 6653183006627102351 2778761554404352279 
5 0 0 10 3 3 
int(7)
int(-43097749724894417)
2f6b0731d3e2667f2685 35dc5d
ecfbe5990a3193804f6b 4fdebe
0.4976636663059516 0.81783844288953311 0.61211189358442264 
bool(true)
Error: Cannot modify readonly property Random\Randomizer::$engine
bool(true)
bool(true)
ValueError: Random\Randomizer::getInt(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)
ValueError: Random\Randomizer::getBytes(): Argument #1 ($length) must be greater than 0
ValueError: Random\Randomizer::getBytes(): Argument #1 ($length) must be greater than 0
