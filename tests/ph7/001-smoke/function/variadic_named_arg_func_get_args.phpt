--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named args absorbed into a variadic are excluded from func_get_args()/func_num_args()
--FILE--
<?php
// Named args absorbed into a variadic are EXCLUDED from func_get_args()/func_num_args()
// (they are not positional), while the variadic param itself keeps them.
function v(...$a) {
    return ['args' => func_get_args(), 'num' => func_num_args(), 'variadic' => $a];
}
var_export(v(...['foo', 'bar', 'biz' => 'kuz']));
echo "\n";

// Mixed: named bound to a declared param stays positional; named into the variadic is excluded.
function m($a, ...$r) {
    return ['args' => func_get_args(), 'num' => func_num_args()];
}
var_export(m(1, x: 2));
echo "\n";

// Plain positional variadic is unchanged.
function p(...$a) { return func_num_args(); }
echo p(1, 2, 3), "\n";
?>
--EXPECT--
array (
  'args' => 
  array (
    0 => 'foo',
    1 => 'bar',
  ),
  'num' => 2,
  'variadic' => 
  array (
    0 => 'foo',
    1 => 'bar',
    'biz' => 'kuz',
  ),
)
array (
  'args' => 
  array (
    0 => 1,
  ),
  'num' => 1,
)
3
