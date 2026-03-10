--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with very large negative length removes nothing
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$r = array_splice($a, 2, -10);
echo count($r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
0
1,2,3,4,5
--CLEAN--
<?php
unset($a, $r);
