--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with null length returns all elements from offset to end
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_slice($a, 1, null);
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
2
3
2
--CLEAN--
<?php
unset($a, $r);
