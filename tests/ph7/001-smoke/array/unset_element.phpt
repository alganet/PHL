--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unsetting specific array entries should decrease the map size and free values
--FILE--
<?php
$a = array('k' => 'v', 1 => 'v1');
echo count($a) . PHP_EOL; // 2
unset($a['k']);
echo count($a) . PHP_EOL; // 1
unset($a[1]);
echo count($a) . PHP_EOL; // 0
?>
--EXPECT--
2
1
0
--CLEAN--
<?php
unset($a);
