--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace()/str_ireplace() apply element-wise to an array subject and return an array
--FILE--
<?php
// An array subject is searched/replaced element-wise and RETURNS AN ARRAY whose
// keys mirror the subject's. Every element is string-cast first (int/float/bool/
// null via their string form).
var_export(str_replace('x', 'y', ['a' => 'xx', 5 => 'x', 'b' => 'no']));
echo "\n";
var_export(str_replace('1', 'Z', [1, 1.5, true, false, null, 'a1b']));
echo "\n";
// Array search + array replace over an array subject.
var_export(str_replace(['a', 'b'], ['X', 'Y'], ['aa', 'bb', 'ab']));
echo "\n";
// Case-insensitive variant.
var_export(str_ireplace('X', 'y', ['xx', 'AXA']));
echo "\n";
// Empty array subject stays an (empty) array.
var_export(str_replace('x', 'y', []));
echo "\n";
?>
--EXPECT--
array (
  'a' => 'yy',
  5 => 'y',
  'b' => 'no',
)
array (
  0 => 'Z',
  1 => 'Z.5',
  2 => 'Z',
  3 => '',
  4 => '',
  5 => 'aZb',
)
array (
  0 => 'XX',
  1 => 'YY',
  2 => 'XY',
)
array (
  0 => 'yy',
  1 => 'AyA',
)
array (
)
--CLEAN--
<?php
