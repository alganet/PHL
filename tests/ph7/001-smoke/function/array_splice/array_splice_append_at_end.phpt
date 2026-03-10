--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with offset equal to count appends replacement at end
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_splice($a, 3, 0, array(4, 5));
echo count($r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
0
1,2,3,4,5
--CLEAN--
<?php
unset($a, $r);
