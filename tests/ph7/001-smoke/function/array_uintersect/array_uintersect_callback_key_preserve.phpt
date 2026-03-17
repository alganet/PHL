--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should preserve keys from the first array
--FILE--
<?php
$a = array(0 => 'a', 1 => 'b', 2 => 'c');
$b = array(0 => 'c', 1 => 'd', 2 => 'a');
$c = array_uintersect($a, $b, function($x, $y) { return strcmp($x, $y); });
foreach ($c as $k => $v) {
    echo $k . ':' . $v . PHP_EOL;
}
?>
--EXPECT--
0:a
2:c
--CLEAN--
<?php
unset($a, $b, $c);
