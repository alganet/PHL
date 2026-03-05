--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reverse reverses a numeric array
--FILE--
<?php
$r = array_reverse(array(1, 2, 3));
echo $r[0] . PHP_EOL;
echo $r[1] . PHP_EOL;
echo $r[2] . PHP_EOL;
?>
--EXPECT--
3
2
1
--CLEAN--
<?php
unset($r);
