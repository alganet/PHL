--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with null callback returns array unchanged
--FILE--
<?php
$r = array_map(null, array(10, 20, 30));
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
echo count($r);
?>
--EXPECT--
10
20
30
3
--CLEAN--
<?php
unset($r);
