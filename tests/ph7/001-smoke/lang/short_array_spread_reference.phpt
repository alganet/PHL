--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array spread preserves by-reference entries
--DESCRIPTION--
Regression: HashmapDuplicateNode value-copied every entry during a merge, so a
reference held by the source array (e.g: [&$x]) was flattened to a plain value.
PHP keeps the reference live across the spread, so mutating the original target
is visible through the new array. Both int-keyed and string-keyed references are
covered, plus a non-reference control that must stay independent.
--FILE--
<?php
$x = 5;
$a = [&$x];
$b = [...$a];
$x = 99;
echo $b[0], "\n";        // 99: reference preserved

$y = 1;
$c = ['k' => &$y];
$d = [...$c];
$y = 7;
echo $d['k'], "\n";      // 7: string-keyed reference preserved

$z = 5;
$e = [$z];               // no reference
$f = [...$e];
$z = 99;
echo $f[0], "\n";        // 5: value copy stays independent
?>
--EXPECT--
99
7
5
--CLEAN--
<?php
unset($x, $a, $b, $y, $c, $d, $z, $e, $f);
