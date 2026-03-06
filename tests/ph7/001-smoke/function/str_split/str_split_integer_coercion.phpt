--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with integer coerces to string and splits
--FILE--
<?php
$r = str_split(123);
echo $r[0] . $r[1] . $r[2] . PHP_EOL;
echo count($r) . PHP_EOL;
?>
--EXPECT--
123
3
--CLEAN--
<?php
unset($r);
