--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect preserves duplicate values from first array
--FILE--
<?php
$a = array(1, 1, 2, 2, 3);
$b = array(1, 2);
$r = array_intersect($a, $b);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:1,1:1,2:2,3:2,
--CLEAN--
<?php
unset($a, $b, $r);
