--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match populates a nested array-subscript by-ref target, COW-correct
--FILE--
<?php
$a = ['x' => ['y' => 0]];
$b = $a; // share
$r = preg_match('/(\w+) (\w+)/', 'Hello World', $a['x']['y']);
echo $r . "\n";
echo $a['x']['y'][0] . "\n";
echo $a['x']['y'][1] . "\n";
var_export($b['x']['y']); echo "\n"; // shared copy untouched (still 0, not an array)
// deeper, fully auto-vivified
preg_match('/(\w+)/', 'Hi', $pmsnC['p']['q']['r']);
echo $pmsnC['p']['q']['r'][0] . "\n";
?>
--EXPECT--
1
Hello World
Hello
0
Hi
--CLEAN--
<?php
