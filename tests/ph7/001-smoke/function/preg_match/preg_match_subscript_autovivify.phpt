--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match auto-vivifies both the array and the element of a subscript by-ref target
--FILE--
<?php
// $a is never declared; the by-ref subscript creates both $a and $a['k'].
$r = preg_match('/(\w+)/', 'Hi', $a['k']);
echo $r . "\n";
echo (is_array($a) ? 'array' : 'not-array') . "\n";
echo $a['k'][0] . "\n";
echo $a['k'][1] . "\n";
?>
--EXPECT--
1
array
Hi
Hi
--CLEAN--
<?php
