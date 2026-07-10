--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
range() PHP 8 value semantics: decreasing, float, char, numeric-string ranges
--FILE--
<?php
echo implode(',', range(10, 1)), "\n";
echo implode(',', range(10, 1, 2)), "\n";
echo implode(',', range(10, 1, -2)), "\n";
echo implode(',', range('z', 't')), "\n";
echo implode(',', range('a', 'e', 2)), "\n";
echo var_export(range('1', '5'), true), "\n";
echo implode(',', range(3.5, 1.5)), "\n";
echo implode(',', range(1, 2, 0.3)), "\n";
echo implode(',', range(9, 1, 2.5)), "\n";
echo implode(',', range(5, 5, 3)), "\n";
echo implode(',', range('c', 'c', 5)), "\n";
echo implode(',', range(-3, 3)), "\n";
echo var_export(range(1, 5, 1.0), true), "\n";
echo var_export(range(1, '5.0'), true), "\n";
echo var_export(range('10', '15'), true), "\n";
echo var_export(range(true, 3), true), "\n";
echo count(range('A', 'z')), "\n";
echo var_export(range(PHP_INT_MAX - 2, PHP_INT_MAX), true), "\n";
echo var_export(range(PHP_INT_MIN, PHP_INT_MIN + 2), true), "\n";
echo var_export(range(-PHP_INT_MAX, PHP_INT_MAX, PHP_INT_MAX), true), "\n";
echo var_export(range(' 5', 6), true), "\n";
echo var_export(range('1e2', '105'), true), "\n";
echo var_export(range(0.1, 0.5, 0.2), true), "\n";
--EXPECT--
10,9,8,7,6,5,4,3,2,1
10,8,6,4,2
10,8,6,4,2
z,y,x,w,v,u,t
a,c,e
array (
  0 => '1',
  1 => '2',
  2 => '3',
  3 => '4',
  4 => '5',
)
3.5,2.5,1.5
1,1.3,1.6,1.9
9,6.5,4,1.5
5
c
-3,-2,-1,0,1,2,3
array (
  0 => 1,
  1 => 2,
  2 => 3,
  3 => 4,
  4 => 5,
)
array (
  0 => 1.0,
  1 => 2.0,
  2 => 3.0,
  3 => 4.0,
  4 => 5.0,
)
array (
  0 => 10,
  1 => 11,
  2 => 12,
  3 => 13,
  4 => 14,
  5 => 15,
)
array (
  0 => 1,
  1 => 2,
  2 => 3,
)
58
array (
  0 => 9223372036854775805,
  1 => 9223372036854775806,
  2 => 9223372036854775807,
)
array (
  0 => -9223372036854775807-1,
  1 => -9223372036854775807,
  2 => -9223372036854775806,
)
array (
  0 => -9223372036854775807,
  1 => 0,
  2 => 9223372036854775807,
)
array (
  0 => 5,
  1 => 6,
)
array (
  0 => 100.0,
  1 => 101.0,
  2 => 102.0,
  3 => 103.0,
  4 => 104.0,
  5 => 105.0,
)
array (
  0 => 0.1,
  1 => 0.30000000000000004,
  2 => 0.5,
)
--CLEAN--
<?php
