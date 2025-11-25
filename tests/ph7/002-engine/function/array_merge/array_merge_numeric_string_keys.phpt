--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge: numeric keys are reindexed and string keys are overwritten by later arrays
--FILE--
<?php
$a = array(5 => 'a', 'k' => 'va');
$b = array(5 => 'b', 'k' => 'vb');
$c = array_merge($a, $b);
// numeric keys should be reindexed: we expect 'a' at index 0 and 'b' at index 1
echo count($c) . PHP_EOL;
echo $c[0] . PHP_EOL;
echo $c[1] . PHP_EOL;
echo $c['k'] . PHP_EOL; // last 'k' wins
?>
--EXPECT--
3
a
b
vb
--CLEAN--
<?php
unset($a, $b, $c);
?>
