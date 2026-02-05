--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ksort should sort arrays by key
--FILE--
<?php
$a = array('c'=>1, 'b'=>2, 'a'=>3);
ksort($a);
foreach($a as $k=>$v) echo $k.':'.$v.PHP_EOL;
?>
--EXPECT--
a:3
b:2
c:1
--CLEAN--
<?php
unset($a);
