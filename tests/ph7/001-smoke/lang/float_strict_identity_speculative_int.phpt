--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
=== on two floats ignores the engine's speculative int representation: (float)"1.0" is identical to 1.0
--FILE--
<?php
// A float built by the (float) cast and a float LITERAL of the same value are one
// type and one value: `===` must not see the engine's speculative integer
// representation, which only one of the two roads leaves behind.
$fsi_c = (float) "1.0";
var_dump($fsi_c === 1.0, 1.0 === $fsi_c, $fsi_c === 1);
var_dump((float) "0.0" === 0.0, (float) "2.0" === 2.0, (float) "-1.0" === -1.0,
         (float) "100.0" === 100.0, (float) "2.5" === 2.5);
var_dump((float) "1" === 1.0, 1e0 === (float) "1", intdiv(4, 2) === 2);
// The same question through every other door that asks for identity.
var_dump(in_array($fsi_c, [1.0], true));
var_dump(array_search($fsi_c, [1.0], true));
var_dump([1.0] === [(float) "1.0"]);
var_dump(array_keys([1.0, 2.0], $fsi_c, true));
var_dump(array_unique([1.0, (float) "1.0"], SORT_REGULAR));
// ...and it must not make an int identical to a float.
var_dump(2 === (float) "2.0", (float) "2.0" === 2);
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
bool(true)
array(1) {
  [0]=>
  int(0)
}
array(1) {
  [0]=>
  float(1)
}
bool(false)
bool(false)
