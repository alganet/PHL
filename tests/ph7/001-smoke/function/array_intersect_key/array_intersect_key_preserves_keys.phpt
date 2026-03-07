--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_key preserves keys from the first array
--FILE--
<?php
$a = array(5 => 'a', 10 => 'b', 15 => 'c');
$b = array(10 => 'x', 15 => 'y', 20 => 'z');
$r = array_intersect_key($a, $b);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
10:b,15:c,
--CLEAN--
<?php
unset($a, $b, $r);
