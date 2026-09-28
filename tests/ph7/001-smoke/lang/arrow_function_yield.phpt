--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An arrow function whose body yields is a generator
--DESCRIPTION--
An arrow body is a single expression, but php takes a `yield` in one: calling
`fn() => yield 7` hands back a Generator, exactly as the `function` form does. Only the
closure/function path scanned its bytecode for the opcode, so the arrow's yield ran with no
generator frame around it and raised `Cannot use yield outside of a generator`. The other
half of the same rule: a yield in a NESTED arrow belongs to the arrow, so the function it
sits in is not a generator on its account.
--FILE--
<?php
$afyA = (fn() => yield 7)();
var_dump($afyA instanceof Generator, $afyA->current());
var_dump((static fn() => yield 8)()->current());
var_dump((fn(): Generator => yield 9)()->current());
var_dump((fn($x) => yield $x)(3)->current());
$afyK = (fn() => yield 'k' => 'v')();
var_dump($afyK->key(), $afyK->current());
var_dump(iterator_to_array((fn() => yield from [1, 2])()));
function afyPlain() { $afyN = fn() => yield 1; return 7; }
var_dump(afyPlain());
function afyBoth() { $afyN = fn() => yield 1; yield 9; }
var_dump(afyBoth()->current());
$afyNot = fn($x) => $x * 2;
var_dump($afyNot(4));
var_dump((new ReflectionFunction(fn() => yield 1))->isGenerator());
?>
--EXPECT--
bool(true)
int(7)
int(8)
int(9)
int(3)
string(1) "k"
string(1) "v"
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
int(7)
int(9)
int(8)
bool(true)
--CLEAN--
<?php
