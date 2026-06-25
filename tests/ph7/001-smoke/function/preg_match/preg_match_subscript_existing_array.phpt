--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match populates an array-subscript by-ref target without touching siblings
--FILE--
<?php
$a = ['x' => 1];
$r = preg_match('/(\w+) (\w+)/', 'Hello World', $a['m']);
echo $r . "\n";
echo $a['m'][0] . "\n";
echo $a['m'][1] . "\n";
echo $a['m'][2] . "\n";
echo $a['x'] . "\n"; // sibling untouched
?>
--EXPECT--
1
Hello World
Hello
World
1
--CLEAN--
<?php
