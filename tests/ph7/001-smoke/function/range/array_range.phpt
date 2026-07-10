--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
range should generate sequences with step and negative ranges
--FILE--
<?php
$r = range(1, 3);
echo implode(',', $r) . PHP_EOL; // 1,2,3
$r2 = range(2, 8, 2);
echo implode(',', $r2) . PHP_EOL; // 2,4,6,8
$r3 = range(3, 1, -1);
echo implode(',', $r3) . PHP_EOL; // 3,2,1
?>
--EXPECT--
1,2,3
2,4,6,8
3,2,1
--CLEAN--
<?php
unset($r, $r2, $r3);
