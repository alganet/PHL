--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge with same variable as both arguments duplicates values
--FILE--
<?php
$a = array(1, 2);
$r = array_merge($a, $a);
echo count($r) . "\n";
echo $r[0] . ',' . $r[1] . ',' . $r[2] . ',' . $r[3];
?>
--EXPECT--
4
1,2,1,2
--CLEAN--
<?php
unset($a, $r);
