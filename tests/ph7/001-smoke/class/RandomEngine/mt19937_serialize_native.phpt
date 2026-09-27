--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An Mt19937 round-trips through __serialize()/__unserialize()
--DESCRIPTION--
A seeded engine is only useful if a program can put it away and take it out
again, so php gives every engine a serialization payload that is its whole
state: a pair whose first half is the (always empty) property table and whose
second is 626 entries -- 624 state words as little-endian hex, then the read
cursor and the mode as plain integers. The cursor is what makes the round trip
exact: an engine 200 words into its block resumes there rather than at the top.
Anything that is not that shape is refused, so a hand-written payload cannot
smuggle a half-initialized generator in.
--FILE--
<?php
$e = new Random\Engine\Mt19937(42);
$payload = $e->__serialize();
var_dump(count($payload), $payload[0], count($payload[1]));
var_dump($payload[1][0], $payload[1][623], $payload[1][624], $payload[1][625]);

/* Move the cursor, then prove it comes back. */
foreach (range(1, 200) as $ignored) {
    $e->generate();
}
$mid = $e->__serialize();
var_dump($mid[1][624]);
$clone = new Random\Engine\Mt19937(0);
$clone->__unserialize($mid);
var_dump($clone->generate() === $e->generate());

/* Same state, same text: serialize() is deterministic. */
var_dump(serialize(new Random\Engine\Mt19937(5)) === serialize(new Random\Engine\Mt19937(5)));
$back = unserialize(serialize(new Random\Engine\Mt19937(77)));
var_dump($back->generate() === (new Random\Engine\Mt19937(77))->generate());

/* __debugInfo() shows exactly the state array and nothing else. */
$info = (new Random\Engine\Mt19937(42))->__debugInfo();
var_dump(array_keys($info), count($info['__states']), $info['__states'][0]);

foreach ([[], [[], []], [[], array_fill(0, 626, 'zz')], [[], [1, 2, 3]]] as $bad) {
    try {
        (new Random\Engine\Mt19937(1))->__unserialize($bad);
        echo "accepted\n";
    } catch (Throwable $t) {
        echo get_class($t), ': ', $t->getMessage(), "\n";
    }
}
?>
--EXPECT--
int(2)
array(0) {
}
int(626)
string(8) "43e9262b"
string(8) "5f42acf3"
int(0)
int(0)
int(200)
bool(true)
bool(true)
bool(true)
array(1) {
  [0]=>
  string(8) "__states"
}
int(626)
string(8) "43e9262b"
Exception: Invalid serialization data for Random\Engine\Mt19937 object
Exception: Invalid serialization data for Random\Engine\Mt19937 object
Exception: Invalid serialization data for Random\Engine\Mt19937 object
Exception: Invalid serialization data for Random\Engine\Mt19937 object
