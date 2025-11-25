--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Sort rehash should renumber numeric keys and result order is sorted values with numeric keys
--FILE--
<?php
$a = array('x' => 2, 'y' => 1);
sort($a);
// Now keys should be reindexed starting at 0 and values sorted
echo count($a) . PHP_EOL;
foreach($a as $v){ echo $v . PHP_EOL; }
?>
--EXPECT--
2
1
2
--CLEAN--
<?php
unset($a);
?>
