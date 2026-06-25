--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match into a subscript COW-separates: a shared copy is left untouched
--FILE--
<?php
$a = ['k' => 0];
$b = $a; // share the array
$r = preg_match('/(\w+)/', 'Hi', $a['k']);
echo $r . "\n";
echo $a['k'][0] . "\n";   // $a was written
var_export($b['k']); echo "\n"; // $b must remain the original 0, not the matches array
?>
--EXPECT--
1
Hi
0
--CLEAN--
<?php
