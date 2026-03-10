--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with more replacement elements than removed inserts extras at position
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$r = array_splice($a, 1, 1, array(10, 20, 30));
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
2
1,10,20,30,3,4,5
--CLEAN--
<?php
unset($a, $r);
