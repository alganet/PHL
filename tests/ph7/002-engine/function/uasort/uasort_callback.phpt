--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
uasort should sort arrays using a user callback while preserving keys
--FILE--
<?php
$a = array('a'=>3,'b'=>1,'c'=>2);
uasort($a, function($x, $y) { return $x - $y; });
foreach($a as $k=>$v) { echo $k.':'.$v . PHP_EOL; }
?>
--EXPECT--
b:1
c:2
a:3
--CLEAN--
<?php
unset($a);
?>
