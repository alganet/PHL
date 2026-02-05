--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
compact returns an associative array of variable names and values
--FILE--
<?php
$a = 'first';
$b = 'second';
$c = compact('a','b');
echo $c['a'] . "\n";
echo $c['b'] . "\n";
?>
--EXPECT--
first
second
--CLEAN--
<?php
unset($a, $b, $c);
