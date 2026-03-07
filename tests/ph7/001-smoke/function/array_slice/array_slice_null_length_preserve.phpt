--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_slice with null length and preserve_keys keeps original keys
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_slice($a, 1, null, true);
$k = array_keys($r);
echo $k[0] . PHP_EOL;
echo $k[1] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
?>
--EXPECT--
1
2
2
3
--CLEAN--
<?php
unset($a, $r, $k);
