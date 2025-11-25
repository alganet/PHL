--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort should sort values in reverse keeping keys
--FILE--
<?php
$a = array('a' => 3, 'b' => 1, 'c' => 2);
arsort($a);
foreach($a as $k=>$v) echo $k.':'.$v.PHP_EOL;
?>
--EXPECT--
a:3
c:2
b:1
--CLEAN--
<?php
unset($a);
?>
