--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionAttribute::__toString(): php's argument block and value-export syntax
--FILE--
<?php
#[Attribute(Attribute::TARGET_ALL)]
class RExpA { public function __construct(...$x) {} }
enum RExpEnum: string { case One = 'one'; }

#[RExpA]
class RExpZero {}
#[RExpA('s', true, false, null, 1.5, 1.0, 100.0, 1e15, 1e-7, 1 / 3, -0.0)]
class RExpScalar {}
#[RExpA([], [1, 2], ['k' => 'v'], [1 => 'a', 0 => 'b'], [[1], [2 => [3]]], ['0' => 'z'])]
class RExpArray {}
#[RExpA("t\tt", "n\nn", "r\rr", "v\x0bv", "f\x0cf", "e\x1be", "z\x00z", "q'q", "b\\b", "h\xff", "\x7f")]
class RExpString {}
#[RExpA(RExpEnum::One, PHP_INT_MAX, -3, 1 + 2, 'a' . 'b')]
class RExpMisc {}
#[RExpA(b: 9, then: 'x')]
class RExpNamed {}

foreach (['RExpZero', 'RExpScalar', 'RExpArray', 'RExpString', 'RExpMisc', 'RExpNamed'] as $c) {
    echo (new ReflectionClass($c))->getAttributes()[0];
}
--EXPECT--
Attribute [ RExpA ]
Attribute [ RExpA ] {
  - Arguments [11] {
    Argument #0 [ 's' ]
    Argument #1 [ true ]
    Argument #2 [ false ]
    Argument #3 [ NULL ]
    Argument #4 [ 1.5 ]
    Argument #5 [ 1.0 ]
    Argument #6 [ 100.0 ]
    Argument #7 [ 1.0E+15 ]
    Argument #8 [ 1.0E-7 ]
    Argument #9 [ 0.33333333333333 ]
    Argument #10 [ -0.0 ]
  }
}
Attribute [ RExpA ] {
  - Arguments [6] {
    Argument #0 [ [] ]
    Argument #1 [ [1, 2] ]
    Argument #2 [ ['k' => 'v'] ]
    Argument #3 [ [1 => 'a', 0 => 'b'] ]
    Argument #4 [ [[1], [2 => [3]]] ]
    Argument #5 [ ['z'] ]
  }
}
Attribute [ RExpA ] {
  - Arguments [11] {
    Argument #0 [ 't\tt' ]
    Argument #1 [ 'n\nn' ]
    Argument #2 [ 'r\rr' ]
    Argument #3 [ 'v\vv' ]
    Argument #4 [ 'f\ff' ]
    Argument #5 [ 'e\ee' ]
    Argument #6 [ 'z\x00z' ]
    Argument #7 [ 'q'q' ]
    Argument #8 [ 'b\\b' ]
    Argument #9 [ 'h\xFF' ]
    Argument #10 [ '\x7F' ]
  }
}
Attribute [ RExpA ] {
  - Arguments [5] {
    Argument #0 [ RExpEnum::One ]
    Argument #1 [ 9223372036854775807 ]
    Argument #2 [ -3 ]
    Argument #3 [ 3 ]
    Argument #4 [ 'ab' ]
  }
}
Attribute [ RExpA ] {
  - Arguments [2] {
    Argument #0 [ b = 9 ]
    Argument #1 [ then = 'x' ]
  }
}
