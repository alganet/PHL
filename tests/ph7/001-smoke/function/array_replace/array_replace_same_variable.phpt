--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace with same variable as both arguments preserves values
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_replace($a, $a);
echo count($r) . "\n";
echo $r[0] . ',' . $r[1] . ',' . $r[2];
?>
--EXPECT--
3
1,2,3
--CLEAN--
<?php
unset($a, $r);
