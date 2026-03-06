--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect preserves original keys from first array
--FILE--
<?php
$a = array(10 => 'a', 20 => 'b', 30 => 'c');
$b = array(1 => 'b', 2 => 'c');
$r = array_intersect($a, $b);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
20:b,30:c,
--CLEAN--
<?php
unset($a, $b, $r);
