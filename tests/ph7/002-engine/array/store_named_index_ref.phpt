--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Store by reference into a named array index
--FILE--
<?php
$a = 1;
$arr = array();
$arr['x'] =& $a; // Store by reference to key 'x'
echo $arr['x'] . "\n";
$a = 7;
echo $arr['x'] . "\n";
?>
--EXPECT--
1
7

--CLEAN--
<?php
unset($a, $arr);
?>
