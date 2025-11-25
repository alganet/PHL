--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort should sort values keeping keys
--FILE--
<?php
$a = array('a' => 3, 'b' => 1, 'c' => 2);
asort($a);
foreach($a as $k=>$v) echo $k.':'.$v.PHP_EOL;
?>
--EXPECT--
b:1
c:2
a:3
--CLEAN--
<?php
unset($a);
?>
