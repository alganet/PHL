--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice removes elements and inserts replacement
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$r = array_splice($a, 1, 2, array(9, 10));
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
2,3
1,9,10,4,5
--CLEAN--
<?php
unset($a, $r);
