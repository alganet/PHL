--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Shuffling, sampling and drawing from an alphabet, with an engine behind them
--DESCRIPTION--
The four methods that shape an engine's bits into DATA rather than a number,
and each one has a rule that only a differential finds. shuffleArray() shuffles
the values and hands back a LIST -- the keys are dropped, not carried along.
pickArrayKeys() is a SAMPLE, not a shuffle: the keys come back in the array's
own order, and asking for more than half of them draws the ones to leave OUT
instead. getBytesFromString() has two implementations, and the boundary is
observable: up to 256 characters it takes the draw APART, masking each byte
down to the alphabet and dropping the ones that overshoot, so an eight-byte
draw usually yields several characters; past 256 an offset no longer fits in a
byte and it falls back to one full ranged draw per character.
--FILE--
<?php
$mt = fn() => new Random\Randomizer(new Random\Engine\Mt19937(1234));

print_r($mt()->shuffleArray([1, 2, 3, 4, 5, 6, 7, 8]));
print_r($mt()->shuffleArray(['x' => 1, 'y' => 2, 'z' => 3]));
var_dump($mt()->shuffleArray([]), $mt()->shuffleArray([9]));
var_dump($mt()->shuffleBytes('abcdefgh'), $mt()->shuffleBytes(''), $mt()->shuffleBytes('q'));

print_r($mt()->pickArrayKeys(['a' => 1, 'b' => 2, 'c' => 3, 'd' => 4, 'e' => 5], 2));
/* Every key, in the array's own order, and its own key TYPES. */
print_r($mt()->pickArrayKeys([5 => 'a', 'x' => 'b', 9 => 'c'], 3));
print_r($mt()->pickArrayKeys(range(1, 10), 8));

var_dump($mt()->getBytesFromString('abcdef', 12));
/* One character is every character. */
var_dump($mt()->getBytesFromString('z', 5));
/* A 256-character alphabet accepts every byte of the draw; a 300-character one
   spends a whole ranged draw per character. */
$all = implode('', array_map(fn($i) => chr($i), range(0, 255)));
var_dump(bin2hex($mt()->getBytesFromString($all, 8)));
$wide = implode('', array_map(fn($i) => chr($i % 256), range(0, 299)));
var_dump(bin2hex($mt()->getBytesFromString($wide, 8)));

foreach ([fn() => $mt()->getBytesFromString('', 5),
          fn() => $mt()->getBytesFromString('ab', 0),
          fn() => $mt()->pickArrayKeys([1, 2, 3], 0),
          fn() => $mt()->pickArrayKeys([1, 2, 3], 4),
          fn() => $mt()->pickArrayKeys([], 1)] as $bad) {
    try { $bad(); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
}

/* An alphabet an engine can never reach is the broken-engine case. */
class SmokeStuckEngine implements Random\Engine
{
    public function generate(): string { return "\xff\xff\xff\xff\xff\xff\xff\xff"; }
}
try { (new Random\Randomizer(new SmokeStuckEngine()))->getBytesFromString('abc', 4); }
catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }

var_dump(extension_loaded('random'));
?>
--EXPECT--
Array
(
    [0] => 3
    [1] => 4
    [2] => 2
    [3] => 7
    [4] => 6
    [5] => 1
    [6] => 5
    [7] => 8
)
Array
(
    [0] => 3
    [1] => 2
    [2] => 1
)
array(0) {
}
array(1) {
  [0]=>
  int(9)
}
string(8) "cdbgfaeh"
string(0) ""
string(1) "q"
Array
(
    [0] => a
    [1] => b
)
Array
(
    [0] => 5
    [1] => x
    [2] => 9
)
Array
(
    [0] => 0
    [1] => 2
    [2] => 3
    [3] => 4
    [4] => 6
    [5] => 7
    [6] => 8
    [7] => 9
)
string(12) "dbdcfcfefbee"
string(5) "zzzzz"
string(16) "2f6b0731d3e2667f"
string(16) "4bab0641744084e1"
ValueError: Random\Randomizer::getBytesFromString(): Argument #1 ($string) must not be empty
ValueError: Random\Randomizer::getBytesFromString(): Argument #2 ($length) must be greater than 0
ValueError: Random\Randomizer::pickArrayKeys(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
ValueError: Random\Randomizer::pickArrayKeys(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)
ValueError: Random\Randomizer::pickArrayKeys(): Argument #1 ($array) must not be empty
Random\BrokenRandomEngineError: Failed to generate an acceptable random number in 50 attempts
bool(true)
