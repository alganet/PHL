--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array assignment copies by value (COW)
--FILE--
<?php
$a = array('x' => 1);
$b = $a; // Copy-on-write
$b['x'] = 2;
echo $a['x'] . PHP_EOL;
echo $b['x'] . PHP_EOL;
?>
--EXPECT--
1
2
--CLEAN--
<?php
unset($a, $b);
