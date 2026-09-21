--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace()/str_ireplace() write the replacement count to the &$count out-param
--FILE--
<?php
// The count accumulates over every search term, and — for an array subject —
// over every element. An undefined variable is created by the by-ref binding.
echo str_replace('a', 'b', 'banana', $c1), " ", var_export($c1, true), "\n";
// No match at all: the count is 0, not left untouched.
echo str_replace('zz', 'b', 'banana', $c2), " ", var_export($c2, true), "\n";
// An empty search term is ignored, so it contributes nothing.
echo str_replace('', 'b', 'banana', $c3), " ", var_export($c3, true), "\n";
// Array search: 'a' matches 3 times, then 'n' twice (replaced with '' since the
// replace array is shorter) — 5 in total.
echo str_replace(['a', 'n'], ['x'], 'banana', $c4), " ", var_export($c4, true), "\n";
// The replacement is not rescanned: 'a' => 'aa' still counts 3.
echo str_replace('a', 'aa', 'banana', $c5), " ", var_export($c5, true), "\n";
// Array subject: the count sums across the elements.
var_export(str_replace('a', 'b', ['banana', 'apple', 5], $c6));
echo " ", var_export($c6, true), "\n";
// Case-insensitive variant counts the same way.
echo str_ireplace('A', 'b', 'banana', $c7), " ", var_export($c7, true), "\n";
// The out-param reaches an array element and an object property too.
$arr = [];
str_replace('a', 'b', 'banana', $arr['k']);
var_export($arr);
echo "\n";
class StrReplaceCountHolder { public $n = 'preset'; }
$o = new StrReplaceCountHolder();
str_replace('n', 'x', 'banana', $o->n);
var_export($o->n);
echo "\n";
// Pre-set variables are overwritten, including on a zero-replacement call.
$c8 = 'preset';
str_replace('q', 'z', 'banana', $c8);
var_export($c8);
echo "\n";
?>
--EXPECT--
bbnbnb 3
banana 0
banana 0
bxxx 5
baanaanaa 3
array (
  0 => 'bbnbnb',
  1 => 'bpple',
  2 => '5',
) 4
bbnbnb 3
array (
  'k' => 3,
)
2
0
--CLEAN--
<?php
