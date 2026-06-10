--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge preserves by-reference entries
--DESCRIPTION--
Regression: array_merge routed every entry through HashmapDuplicateNode, which
value-copied references and lost the by-reference binding. PHP keeps references
live through the merge.
--FILE--
<?php
$x = 5;
$a = [&$x];
$b = array_merge($a, [7]);
$x = 99;
echo $b[0], ',', $b[1], "\n";   // 99,7
?>
--EXPECT--
99,7
--CLEAN--
<?php
unset($x, $a, $b);
