--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge merges multiple short-array literal arguments
--DESCRIPTION--
Regression: a short-array literal [...] is extracted as a single self-contained
node whose opening '[' had no balancing close-bracket node, so the argument
splitter's nesting counter never returned to zero. Every comma after the first
literal was hidden, folding all arguments into one and leaving only the last
literal (array_merge([1],[2]) wrongly returned [2]). Using array() or variables
masked the bug. PHP merges every argument.
--FILE--
<?php
$two = array_merge([1], [2]);
echo count($two), ':', implode(',', $two), "\n";

$three = array_merge([1], [2], [3]);
echo count($three), ':', implode(',', $three), "\n";

$keys = array_merge(['a' => 1], ['b' => 2]);
echo $keys['a'], $keys['b'], "\n";

// Inner literal must stay nested, not be flattened by the splitter.
$nested = array_merge([1, [9]], [2]);
echo count($nested), ':', $nested[0], ',', $nested[1][0], ',', $nested[2], "\n";

// Mixed literal + variable, both orders, must still work.
$v = [2];
echo implode(',', array_merge([1], $v)), "\n";
echo implode(',', array_merge($v, [1])), "\n";

// No regression for a single literal or an empty literal.
echo implode(',', array_merge([1])), "\n";
echo implode(',', array_merge([], [1])), "\n";
?>
--EXPECT--
2:1,2
3:1,2,3
12
3:1,9,2
1,2
2,1
1
1
--CLEAN--
<?php
unset($two, $three, $keys, $nested, $v);
