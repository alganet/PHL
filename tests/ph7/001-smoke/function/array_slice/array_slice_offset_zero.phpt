--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with offset zero and no length returns full array
--FILE--
<?php
$a = array(10, 20, 30);
$r = array_slice($a, 0);
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
10
20
30
3
--CLEAN--
<?php
unset($a, $r);
