--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf %u/%x/%X/%o/%b reinterpret the 64 bits; %d of PHP_INT_MIN has no magnitude
--FILE--
<?php
// Only %d is signed. The other radix conversions cast to an unsigned 64-bit
// integer, so a negative value prints its two's-complement bit pattern rather
// than its magnitude.
foreach ([-1, -255, -8, PHP_INT_MIN, PHP_INT_MIN + 1, PHP_INT_MAX, 0, 42] as $sprintfRadixV) {
    echo $sprintfRadixV, "\n";
    echo '  d=', sprintf('%d', $sprintfRadixV), "\n";
    echo '  u=', sprintf('%u', $sprintfRadixV), "\n";
    echo '  x=', sprintf('%x', $sprintfRadixV), "\n";
    echo '  X=', sprintf('%X', $sprintfRadixV), "\n";
    echo '  o=', sprintf('%o', $sprintfRadixV), "\n";
    echo '  b=', sprintf('%b', $sprintfRadixV), "\n";
}
// The magnitude of PHP_INT_MIN is not representable as a signed int, and the
// digit loop used to index its charset with a negative remainder.
echo sprintf('%020d|%+d|%5x', PHP_INT_MIN, PHP_INT_MIN, PHP_INT_MIN), "\n";
--EXPECT--
-1
  d=-1
  u=18446744073709551615
  x=ffffffffffffffff
  X=FFFFFFFFFFFFFFFF
  o=1777777777777777777777
  b=1111111111111111111111111111111111111111111111111111111111111111
-255
  d=-255
  u=18446744073709551361
  x=ffffffffffffff01
  X=FFFFFFFFFFFFFF01
  o=1777777777777777777401
  b=1111111111111111111111111111111111111111111111111111111100000001
-8
  d=-8
  u=18446744073709551608
  x=fffffffffffffff8
  X=FFFFFFFFFFFFFFF8
  o=1777777777777777777770
  b=1111111111111111111111111111111111111111111111111111111111111000
-9223372036854775808
  d=-9223372036854775808
  u=9223372036854775808
  x=8000000000000000
  X=8000000000000000
  o=1000000000000000000000
  b=1000000000000000000000000000000000000000000000000000000000000000
-9223372036854775807
  d=-9223372036854775807
  u=9223372036854775809
  x=8000000000000001
  X=8000000000000001
  o=1000000000000000000001
  b=1000000000000000000000000000000000000000000000000000000000000001
9223372036854775807
  d=9223372036854775807
  u=9223372036854775807
  x=7fffffffffffffff
  X=7FFFFFFFFFFFFFFF
  o=777777777777777777777
  b=111111111111111111111111111111111111111111111111111111111111111
0
  d=0
  u=0
  x=0
  X=0
  o=0
  b=0
42
  d=42
  u=42
  x=2a
  X=2A
  o=52
  b=101010
-9223372036854775808|-9223372036854775808|8000000000000000
--CLEAN--
<?php
