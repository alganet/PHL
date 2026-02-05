--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse should reverse numeric arrays and optionally preserve keys for associative arrays
--FILE--
<?php
$a = array(1,2,3);
$r = array_reverse($a);
echo implode(',', $r) . PHP_EOL; // 3,2,1
 $assoc = array('a'=>1,'b'=>2);
 $p = array_reverse($assoc, true);
echo implode(',', array_keys($p)) . PHP_EOL; // b,a
?>
--EXPECT--
3,2,1
b,a
--CLEAN--
<?php
unset($a, $r, $assoc, $p);
