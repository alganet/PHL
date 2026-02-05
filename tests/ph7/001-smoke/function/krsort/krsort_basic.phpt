--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
krsort should sort keys in reverse
--FILE--
<?php
$a = array('a'=>3, 'b'=>2, 'c'=>1);
krsort($a);
foreach($a as $k=>$v) echo $k.':'.$v.PHP_EOL;
?>
--EXPECT--
c:1
b:2
a:3
--CLEAN--
<?php
unset($a);
