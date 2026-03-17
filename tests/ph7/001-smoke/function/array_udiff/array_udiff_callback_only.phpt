--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should return the first array if only array and callback are provided
--FILE--
<?php
$a = array(1, 2);
$b = array_udiff($a, function($x, $y) { return $x - $y; });
echo count($b) . PHP_EOL;
foreach ($b as $k => $v) {
    echo $k . ':' . $v . PHP_EOL;
}
?>
--EXPECT--
2
0:1
1:2
--CLEAN--
<?php
unset($a, $b);
