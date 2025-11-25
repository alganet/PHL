--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace: values from later arrays overwrite earlier ones and numeric keys are preserved (not renumbered)
--FILE--
<?php
$a = array(5 => 'a', 'k' => 'va');
$b = array(5 => 'b', 'k' => 'vb');
$r = array_replace($a, $b);
// numeric key (5) should be overwritten by 'b'
// string key 'k' should be overwritten by 'vb'
echo count($r) . PHP_EOL;
echo $r[5] . PHP_EOL;
echo $r['k'] . PHP_EOL;
?>
--EXPECT--
2
b
vb
--CLEAN--
<?php
unset($a, $b, $r);
?>
