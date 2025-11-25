--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usort should order array values using a user callback
--FILE--
<?php
$a = array(3,1,2);
usort($a, function($x, $y) { return $x - $y; });
foreach($a as $v) { echo $v . PHP_EOL; }
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
unset($a);
?>
